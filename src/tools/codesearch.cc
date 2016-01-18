/********************************************************************
 * livegrep -- tools/codesearch.cc
 * Copyright (c) 2011-2014 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "codesearch.h"
#include "timer.h"
#include "metrics.h"
#include "re_width.h"
#include "debug.h"
#include "git_indexer.h"
#include "fs_indexer.h"

#include "transport.h"

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <semaphore.h>

#include <iostream>

#include <gflags/gflags.h>

#include <re2/regexp.h>
#include "re2/walker-inl.h"

#include <json/json.h>

DEFINE_int32(concurrency, 16, "Number of concurrent queries to allow.");
DEFINE_string(dump_index, "", "Dump the produced index to a specified file");
DEFINE_string(load_index, "", "Load the index from a file instead of walking the repository");
DEFINE_bool(quiet, false, "Do the search, but don't print results.");
DEFINE_string(listen, "", "Listen on a socket for connections. example: -listen tcp://localhost:9999");

using namespace std;
using namespace re2;

void die_errno(const char *str) {
    perror(str);
    exit(1);
}

struct print_match {
    print_match(codesearch_transport *tx) : tx_(tx) {}

    void operator()(const match_result *m) const {
        if (FLAGS_quiet)
            return;
        tx_->write_match(m);
    }
protected:
    codesearch_transport *tx_;
};

const int kMaxProgramSize = 4000;
const int kMaxWidth       = 200;

sem_t interact_sem;

void interact(code_searcher *cs, codesearch_transport *tx) {
    code_searcher::search_thread search(cs);
    WidthWalker width;

    index_info info;
    info.name = cs->name();
    info.trees = cs->trees();
    bool done = false;

    while (!done) {
        tx->write_ready(&info);

        query q;
        if (!tx->read_query(&q, &done))
            continue;

        log(q.trace_id,
            "processing query line='%s' file='%s' tree='%s' not_file='%s' not_tree='%s'",
            q.line_pat->pattern().c_str(),
            q.file_pat->pattern().c_str(),
            q.tree_pat->pattern().c_str(),
            q.negate.file_pat->pattern().c_str(),
            q.negate.tree_pat->pattern().c_str());

        if (q.line_pat->ProgramSize() > kMaxProgramSize) {
            log(q.trace_id, "program too large size=%d", q.line_pat->ProgramSize());
            tx->write_error("Parse error.");
            continue;
        }
        int w = width.Walk(q.line_pat->Regexp(), 0);
        if (w > kMaxWidth) {
            log(q.trace_id, "program too wide width=%d", w);
            tx->write_error("Parse error.");
            continue;
        }
        {
            timer tm;
            struct timeval elapsed;
            match_stats stats;

            {
                sem_wait(&interact_sem);
                search.match(q, print_match(tx), &stats);
                sem_post(&interact_sem);
            }
            elapsed = tm.elapsed();
            tx->write_done(elapsed, &stats);
            log(q.trace_id, "done elapsed=%ld", timeval_ms(elapsed));
        }
    }
}

void build_index(code_searcher *cs, const vector<std::string> &argv) {
    if (argv.size() != 2) {
        fprintf(stderr, "Usage: %s [OPTIONS] config.json\n", argv[0].c_str());
        exit(1);
    }
    json_object *obj = json_object_from_file(const_cast<char*>(argv[1].c_str()));
    if (is_error(obj)) {
        fprintf(stderr, "Error parsing `%s': %s\n",
                argv[1].c_str(), json_tokener_errors[-(unsigned long)obj]);
        exit(1);
    }
    index_spec spec;
    json_parse_error err = parse_index_spec(obj, &spec);
    if (!err.ok()) {
        fprintf(stderr, "parsing %s: %s\n", argv[1].c_str(), err.err().c_str());
        exit(1);
    }
    if (spec.paths.empty() && spec.repos.empty()) {
        fprintf(stderr, "%s: You must specify at least one path to index.\n", argv[1].c_str());
        exit(1);
    }
    json_object_put(obj);
    if (spec.name.size())
        cs->set_name(spec.name);
    for (auto it = spec.paths.begin(); it != spec.paths.end(); ++it) {
        fprintf(stderr, "Walking path_spec name=%s, path=%s\n",
                it->name.c_str(), it->path.c_str());
        fs_indexer indexer(cs, it->path, it->name, it->metadata);
        indexer.walk(it->path);
        fprintf(stderr, "done\n");
    }

    for (auto it = spec.repos.begin(); it != spec.repos.end(); ++it) {
        fprintf(stderr, "Walking repo_spec name=%s, path=%s\n",
                it->name.c_str(), it->path.c_str());
        git_indexer indexer(cs, it->path, it->name, it->metadata);
        for (auto rev = it->revisions.begin();
             rev != it->revisions.end(); ++rev) {
            fprintf(stderr, "  walking %s... ", rev->c_str());
            indexer.walk(*rev);
            fprintf(stderr, "done\n");
        }
    }
}

void initialize_search(code_searcher *search,
                       int argc, char **argv) {
    if (FLAGS_load_index.size() == 0) {
        if (FLAGS_dump_index.size())
            search->set_alloc(make_dump_allocator(search, FLAGS_dump_index));
        else
            search->set_alloc(make_mem_allocator());

        vector<std::string> args;
        for (int i = 0; i < argc; ++i)
            args.push_back(argv[i]);

        timer tm;
        struct timeval elapsed;
        build_index(search, args);
        fprintf(stderr, "Finalizing...\n");
        search->finalize();
        elapsed = tm.elapsed();
        fprintf(stderr, "repository indexed in %d.%06ds\n",
                (int)elapsed.tv_sec, (int)elapsed.tv_usec);
        metric::dump_all();
    } else {
        search->load_index(FLAGS_load_index);
    }
    if (FLAGS_dump_index.size() && FLAGS_load_index.size())
        search->dump_index(FLAGS_dump_index);
}

struct child_state {
    int fd;
    code_searcher *search;
};

void *handle_client(void *data) {
    child_state *child = static_cast<child_state*>(data);
    FILE *r = fdopen(child->fd, "r");
    FILE *w = fdopen(dup(child->fd), "w");

    union {
        struct sockaddr addr;
        struct sockaddr_in addr_in;
        struct sockaddr_un addr_un;
    } addr;
    socklen_t socklen = sizeof(addr);

    if (getpeername(child->fd, &addr.addr, &socklen) == 0) {
        if (addr.addr.sa_family == AF_INET) {
            char name[256];
            printf("connection received from %s:%d\n",
                   inet_ntop(addr.addr.sa_family, &addr.addr_in.sin_addr,
                             name, sizeof(name)),
                   int(addr.addr_in.sin_port));
        }
    }


    codesearch_transport *tx = new codesearch_transport(r, w);
    interact(child->search, tx);
    delete tx;
    delete child;
    fclose(r);
    fclose(w);
    return 0;
}

int bind_to_address(string spec) {
    int off = spec.find("://");
    string proto, address;

    if (off == string::npos) {
        proto = "unix";
        address = spec;
    } else {
        proto = spec.substr(0, off);
        address = spec.substr(off + 3);
    }

    int server;

    if (proto == "unix") {
        server = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server < 0)
            die_errno("socket(AF_UNIX)");

        struct sockaddr_un addr;

        memset(&addr, 0, sizeof addr);
        addr.sun_family = AF_UNIX;
        memcpy(addr.sun_path, address.c_str(), min(address.size(), sizeof(addr.sun_path) - 1));

        if (::bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0)
            die_errno("Unable to bind socket");
    } else if (proto == "tcp") {
        int colon = address.find(':');
        if (colon == string::npos) {
            die("-listen: TCP addresses must be HOST:PORT.");
        }
        string host = address.substr(0, colon);
        struct addrinfo hint = {};
        hint.ai_family = AF_INET;
        hint.ai_socktype = SOCK_STREAM;

        struct addrinfo *addrs = NULL;
        int err;
        if ((err = getaddrinfo(host.c_str(), address.c_str() + colon + 1,
                               &hint, &addrs)) != 0) {
            die("Error resolving %s: %s", host.c_str(), gai_strerror(err));
        }

        server = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
        if (server < 0)
            die_errno("Creating socket");
        int one = 1;
        if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0)
            die_errno("set reuseaddr");
        if (::bind(server, addrs->ai_addr, addrs->ai_addrlen) != 0)
            die_errno("Binding to address");
        freeaddrinfo(addrs);
    } else {
        die("Unknown protocol: %s", proto.c_str());
    }

    if (listen(server, 4) < 0)
        die_errno("listen()");

    return server;
}

void listen(code_searcher *search, string path) {
    int server = bind_to_address(path);

    printf("codesearch: listening on %s.\n", path.c_str());

    while(1) {
        int fd = accept(server, NULL, NULL);
        if (fd < 0)
            die_errno("accept");

        child_state *state = new child_state;
        state->fd = fd;
        state->search = search;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, state);
    }
}

int main(int argc, char **argv) {
    google::SetUsageMessage("Usage: " + string(argv[0]) + " <options> REFS");
    google::ParseCommandLineFlags(&argc, &argv, true);

    prctl(PR_SET_PDEATHSIG, SIGINT);

    code_searcher search;

    signal(SIGPIPE, SIG_IGN);

    initialize_search(&search, argc, argv);

    if (sem_init(&interact_sem, 0, FLAGS_concurrency) < 0)
        die_errno("sem_init");

    if (FLAGS_listen.size()) {
        listen(&search, FLAGS_listen);
    } else {
        codesearch_transport *tx = new codesearch_transport(stdin, stdout);
        interact(&search, tx);
        delete tx;
    }

    return 0;
}
