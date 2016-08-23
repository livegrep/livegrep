/********************************************************************
 * livegrep -- tools/codesearch.cc
 * Copyright (c) 2011-2014 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "src/lib/timer.h"
#include "src/lib/metrics.h"
#include "src/lib/debug.h"

#include "src/codesearch.h"
#include "src/tagsearch.h"
#include "src/re_width.h"
#include "src/git_indexer.h"
#include "src/fs_indexer.h"

#include "src/tools/transport.h"
#include "src/tools/limits.h"
#include "src/tools/grpc_server.h"

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <semaphore.h>

#include <iostream>
#include <functional>
#include <thread>

#include <gflags/gflags.h>

#include <boost/bind.hpp>
#include "re2/regexp.h"
#include "re2/walker-inl.h"

#include <json-c/json.h>

#include <grpc++/server.h>
#include <grpc++/server_builder.h>

DEFINE_int32(concurrency, 16, "Number of concurrent queries to allow.");
DEFINE_string(dump_index, "", "Dump the produced index to a specified file");
DEFINE_string(load_index, "", "Load the index from a file instead of walking the repository");
DEFINE_string(load_tags, "", "Load the index built from a tags file.");
DEFINE_bool(quiet, false, "Do the search, but don't print results.");
DEFINE_string(listen, "", "Listen on a socket for connections. example: -listen tcp://localhost:9999");
DEFINE_string(grpc, "", "Listen for GRPC clients. example: -grpc localhost:9999");
DEFINE_string(listen_tags, "", "Listen on a socket for connections to tag search. example: -listen_tags tcp://localhost:9998");

using namespace std;
using namespace re2;

using grpc::Server;
using grpc::ServerBuilder;

sem_t interact_sem;

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

typedef std::function<match_stats (code_searcher::search_thread*,
                                   query*,
                                   codesearch_transport*)> match_func;

struct codesearch_matcher {
    match_stats operator()(code_searcher::search_thread *s, query *q, codesearch_transport *tx) {
        match_stats stats;
        sem_wait(&interact_sem);
        s->match(*q, print_match(tx), &stats);
        sem_post(&interact_sem);
        return stats;
    }
};

struct tagsearch_matcher {
    tagsearch_matcher(tag_searcher* ts) : ts_(ts) {}

    match_stats operator()(code_searcher::search_thread *s, query *q, codesearch_transport *tx) {
        match_stats stats;
        // the negation constraints will be checked when we transform the match
        // (unfortunately, we can't construct a line query that checks these)
        query constraints;
        constraints.negate.file_pat.swap(q->negate.file_pat);
        constraints.negate.tags_pat.swap(q->negate.tags_pat);

        // modify the line pattern to match the constraints that we can handle now
        std::string regex = tag_searcher::create_tag_line_regex_from_query(q);
        q->line_pat.reset(new RE2(regex, q->line_pat->options()));
        q->file_pat.reset();
        q->tags_pat.reset();

        sem_wait(&interact_sem);
        s->match(*q,
                 print_match(tx),
                 boost::bind(&tag_searcher::transform, ts_, &constraints, _1),
                 &stats);
        sem_post(&interact_sem);
        return stats;
    }
protected:
    tag_searcher* ts_;
};

std::string pat(const std::unique_ptr<RE2> &p) {
    if (p.get() == 0)
        return "";
    return p->pattern();
}

void interact(code_searcher *cs, codesearch_transport *tx, const match_func& match) {
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
            "processing query line='%s' file='%s' tree='%s' tags='%s' "
                                        "not_file='%s' not_tree='%s' not_tags='%s'",
            pat(q.line_pat).c_str(),
            pat(q.file_pat).c_str(),
            pat(q.tree_pat).c_str(),
            pat(q.tags_pat).c_str(),
            pat(q.negate.file_pat).c_str(),
            pat(q.negate.tree_pat).c_str(),
            pat(q.negate.tags_pat).c_str());

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
            match_stats stats = match(&search, &q, tx);
            elapsed = tm.elapsed();
            tx->write_done(elapsed, &stats);
            log(q.trace_id, "done elapsed=%ld matches=%d why=%d",
                timeval_ms(elapsed), stats.matches, int(stats.why));
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
        fprintf(stderr, "Error parsing `%s'\n",
                argv[1].c_str());
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
                       tag_searcher *tags,
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
    if (FLAGS_load_tags.size() != 0) {
        tags->load_index(FLAGS_load_tags);
        tags->cache_indexed_files(search);
    }
    if (FLAGS_dump_index.size() && FLAGS_load_index.size())
        search->dump_index(FLAGS_dump_index);
}

struct child_state {
    int fd;
    code_searcher *search;
    match_func match;
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
    interact(child->search, tx, child->match);
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

void listen(code_searcher *search, const string& path, const match_func& match) {
    int server = bind_to_address(path);

    printf("codesearch: listening on %s.\n", path.c_str());

    while(1) {
        int fd = accept(server, NULL, NULL);
        if (fd < 0)
            die_errno("accept");

        child_state *state = new child_state;
        state->fd = fd;
        state->search = search;
        state->match = match;

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, state);
    }
}

void listen_grpc(code_searcher *search, const string& addr) {
    CodeSearchImpl service(search);

    ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    server->Wait();
}

int main(int argc, char **argv) {
    gflags::SetUsageMessage("Usage: " + string(argv[0]) + " <options> REFS");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    prctl(PR_SET_PDEATHSIG, SIGINT);

    code_searcher search;
    tag_searcher tags;

    signal(SIGPIPE, SIG_IGN);

    initialize_search(&search, &tags, argc, argv);

    if (sem_init(&interact_sem, 0, FLAGS_concurrency) < 0)
        die_errno("sem_init");

    std::vector<std::thread> listeners;
    if (FLAGS_grpc.size()) {
        listeners.emplace_back(
            std::thread(boost::bind(&listen_grpc, &search, FLAGS_grpc)));
    }
    if (FLAGS_listen.size()) {
        codesearch_matcher matcher;
        listeners.emplace_back(
            std::thread(boost::bind(&listen, &search, FLAGS_listen, matcher)));
    }
    if (FLAGS_listen_tags.size()) {
        tagsearch_matcher matcher(&tags);
        listeners.emplace_back(
            std::thread(boost::bind(&listen, tags.cs(), FLAGS_listen_tags, matcher)));
    }
    for (auto& listener : listeners) {
        listener.join();
    }

    if (listeners.size() == 0) {
        codesearch_transport *tx = new codesearch_transport(stdin, stdout);
        interact(&search, tx, codesearch_matcher());
        delete tx;
    }

    return 0;
}
