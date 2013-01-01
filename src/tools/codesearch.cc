/********************************************************************
 * livegrep -- main.cc
 * Copyright (c) 2011-2012 Nelson Elhage
 * All Rights Reserved
 ********************************************************************/
#include "codesearch.h"
#include "git_indexer.h"
#include "timer.h"
#include "re_width.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <semaphore.h>

#include <iostream>

#include <gflags/gflags.h>

#include <json/json.h>

#include <re2/regexp.h>
#include "re2/walker-inl.h"

DEFINE_bool(json, false, "Use JSON output.");
DEFINE_int32(concurrency, 16, "Number of concurrent queries to allow.");
DEFINE_string(dump_index, "", "Dump the produced index to a specified file");
DEFINE_string(load_index, "", "Load the index from a file instead of walking the repository");
DEFINE_bool(quiet, false, "Do the search, but don't print results.");
DEFINE_string(listen, "", "Listen on a UNIX socket for connections");
DEFINE_string(file, "", "Only match files matching the provided regex");

using namespace std;
using namespace re2;

void die_errno(const char *str) {
    perror(str);
    exit(1);
}

long timeval_ms (struct timeval tv) {
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

json_object *to_json(const char *str) {
    return json_object_new_string(str);
}

json_object *to_json(const string &str) {
    return json_object_new_string(str.c_str());
}

json_object *to_json(const StringPiece &str) {
    return json_object_new_string_len(str.data(),
                                      str.size());
}

json_object *to_json(int i) {
    return json_object_new_int(i);
}

template <class T>
json_object *to_json(vector<T> vec) {
    json_object *out = json_object_new_array();
    for (auto it = vec.begin(); it != vec.end(); it++)
        json_object_array_add(out, to_json(*it));
    return out;
}

static json_object *to_json(const indexed_path &path) {
    json_object *out = json_object_new_object();
    json_object_object_add(out, "ref",  to_json(path.tree->name));
    json_object_object_add(out, "path", to_json(path.path));
    return out;
}

struct print_match {
    print_match(FILE *out) : out_(out) {}

    void print(const match_result *m) const {
        for (auto ctx = m->context.begin();
             ctx != m->context.end(); ++ctx) {
            for (auto it = ctx->paths.begin(); it != ctx->paths.end(); ++it) {
                fprintf(out_,
                        "%s:%s:%d:%d-%d: %.*s\n",
                        it->tree->name.c_str(),
                        it->path.c_str(),
                        ctx->lno,
                        m->matchleft, m->matchright,
                        m->line.size(), m->line.data());
            }
        }
    }

    void print_json(const match_result *m) const {
        json_object *obj = json_object_new_object();
        json_object_object_add(obj, "ref",
                               to_json(m->context[0].paths[0].tree->name));
        json_object_object_add(obj, "file",
                               to_json(m->context[0].paths[0].path));
        json_object *contexts = json_object_new_array();
        for (auto ctx = m->context.begin();
             ctx != m->context.end(); ++ctx) {
            json_object *jctx = json_object_new_object();
            json_object_object_add(jctx, "paths",  to_json(ctx->paths));
            json_object_object_add(jctx, "lno", to_json(ctx->lno));
            json_object_object_add(jctx, "context_before",
                                   to_json(ctx->context_before));
            json_object_object_add(jctx, "context_after",
                                   to_json(ctx->context_after));
            json_object_array_add(contexts, jctx);
        }
        json_object_object_add(obj, "contexts", contexts);
        json_object_object_add(obj, "lno",  json_object_new_int(m->context[0].lno));
        json_object *bounds = json_object_new_array();
        json_object_array_add(bounds, to_json(m->matchleft));
        json_object_array_add(bounds, to_json(m->matchright));
        json_object_object_add(obj, "bounds", bounds);
        json_object_object_add(obj, "line", to_json(m->line));
        json_object_object_add(obj, "context_before",
                               to_json(m->context[0].context_before));
        json_object_object_add(obj, "context_after",
                               to_json(m->context[0].context_after));
        fprintf(out_, "%s\n", json_object_to_json_string(obj));
        json_object_put(obj);
    }

    void operator()(const match_result *m) const {
        if (FLAGS_quiet)
            return;
        if (FLAGS_json)
            print_json(m);
        else
            print(m);
    }
protected:
    FILE *out_;
};

void print_stats(FILE *out, const match_stats &stats) {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "re2_time", json_object_new_int
                           (timeval_ms(stats.re2_time)));
    json_object_object_add(obj, "git_time", json_object_new_int
                           (timeval_ms(stats.git_time)));
    json_object_object_add(obj, "sort_time", json_object_new_int
                           (timeval_ms(stats.sort_time)));
    json_object_object_add(obj, "index_time", json_object_new_int
                           (timeval_ms(stats.index_time)));
    json_object_object_add(obj, "analyze_time", json_object_new_int
                           (timeval_ms(stats.analyze_time)));
    switch(stats.why) {
    case kExitNone: break;
    case kExitMatchLimit:
        json_object_object_add(obj, "why", json_object_new_string("limit"));
        break;
    case kExitTimeout:
        json_object_object_add(obj, "why", json_object_new_string("timeout"));
        break;
    }
    fprintf(out, "DONE %s\n", json_object_to_json_string(obj));
    json_object_put(obj);
}

void print_error(FILE *out, const string& err) {
    if (!FLAGS_json)
        fprintf(out, "Error: %s\n", err.c_str());
    else
        fprintf(out, "FATAL %s\n", err.c_str());
}

const int kMaxProgramSize = 4000;
const int kMaxWidth       = 200;

bool getline(FILE *stream, string &out) {
    char *line = 0;
    size_t n = 0;
    ssize_t r;
    r = getline(&line, &n, stream);
    if (r == 0 || r == -1)
        out.clear();
    else
        out.assign(line, r - 1);

    return r != -1;
}

bool parse_input(FILE *out, string in, string& line_re, string& file_re)
{
    json_object *js = json_tokener_parse(in.c_str());
    if (is_error(js)) {
        print_error(out, "Parse error: " +
                    string(json_tokener_errors[-(unsigned long)js]));
        return false;
    }
    if (json_object_get_type(js) != json_type_object) {
        print_error(out, "Expected a JSON object");
        return false;
    }

    json_object *line_js = json_object_object_get(js, "line");
    if (!line_js || json_object_get_type(line_js) != json_type_string) {
        print_error(out, "No regex specified!");
        return false;
    }
    line_re = json_object_get_string(line_js);

    json_object *file_js = json_object_object_get(js, "file");
    if (file_js && json_object_get_type(file_js) == json_type_string)
        file_re = json_object_get_string(file_js);
    else
        file_re = FLAGS_file;

    json_object_put(js);

    return true;
}

sem_t interact_sem;

void interact(code_searcher *cs, FILE *in, FILE *out) {
    code_searcher::search_thread search(cs);
    WidthWalker width;

    assert(in != out);
    assert(!setvbuf(in,  NULL, _IOFBF, 4096*4));
    assert(!setvbuf(out, NULL, _IONBF, 0));

    RE2::Options opts;
    opts.set_never_nl(true);
    opts.set_one_line(false);
    opts.set_perl_classes(true);
    opts.set_word_boundary(true);
    opts.set_posix_syntax(true);
    opts.set_word_boundary(true);
    opts.set_log_errors(false);

    while (true) {
        if (FLAGS_json)
            fprintf(out, "READY\n");
        else {
            fprintf(out, "regex> ");
            fflush(out);
        }
        string input;
        if (!getline(in, input)) {
            break;
        }
        if (feof(in) || ferror(in)) {
            break;
        }

        string line, file;
        if (!FLAGS_json) {
            line = input;
            file = FLAGS_file;
        } else {
            if (!parse_input(out, input, line, file))
                continue;
        }

        RE2 re(line, opts);
        RE2 file_re(file, opts);
        if (!re.ok()) {
            print_error(out, re.error());
            continue;
        }
        if (!file_re.ok()) {
            print_error(out, file_re.error());
            continue;
        }
        if (re.ProgramSize() > kMaxProgramSize) {
            print_error(out, "Parse error.");
            continue;
        }
        int w = width.Walk(re.Regexp(), 0);
        if (w > kMaxWidth) {
            print_error(out, "Parse error.");
            continue;
        }
        {
            timer tm;
            struct timeval elapsed;
            match_stats stats;

            if (!FLAGS_json)
                fprintf(out, "ProgramSize: %d\n", re.ProgramSize());

            {
                sem_wait(&interact_sem);
                search.match(re, file.size() ? &file_re : 0, print_match(out), &stats);
                sem_post(&interact_sem);
            }
            elapsed = tm.elapsed();
            if (FLAGS_json)
                print_stats(out, stats);
            else {
                fprintf(out,
                        "Match completed in %d.%06ds.",
                        (int)elapsed.tv_sec, (int)elapsed.tv_usec);
                switch (stats.why) {
                case kExitNone:
                    fprintf(out, "\n");
                    break;
                case kExitMatchLimit:
                    fprintf(out, " (match limit)\n");
                    break;
                case kExitTimeout:
                    fprintf(out, " (timeout)\n");
                    break;
                }
            }
        }
    }
}

struct parse_spec {
    string path;
    string name;
    vector<string> revs;
};

parse_spec parse_walk_spec(string spec) {
    /* [name@]path[:rev1,rev2,rev3] */
    parse_spec out;
    int idx;
    if ((idx = spec.find('@')) != -1) {
        out.name = spec.substr(0, idx);
        spec = spec.substr(idx + 1);
    }
    if ((idx = spec.find(':')) != -1) {
        string revs = spec.substr(idx + 1);
        spec = spec.substr(0, idx);
        while ((idx = revs.find(',')) != -1) {
            out.revs.push_back(revs.substr(0, idx));
            revs = revs.substr(idx + 1);
        }
        if (revs.size())
            out.revs.push_back(revs);
    }
    if (out.revs.empty()) {
        out.revs.push_back("HEAD");
    }
    out.path = spec;
    return out;
}

void walk_one(code_searcher *search, string spec) {
    parse_spec parsed = parse_walk_spec(spec);
    if (!FLAGS_json)
        printf("Walking `%s' (name: %s, path: %s)...\n",
               spec.c_str(),
               parsed.name.c_str(),
               parsed.path.c_str());
    git_indexer indexer(search, parsed.path, parsed.name);
    for (auto it = parsed.revs.begin(); it != parsed.revs.end(); ++it) {
        if (!FLAGS_json) {
            printf("  %s...", it->c_str());
            fflush(stdout);
        }
        indexer.walk(*it);
        if (!FLAGS_json)
            printf("done\n");
    }
}

void initialize_search(code_searcher *search, int argc, char **argv) {
    if (FLAGS_load_index.size() == 0) {
        if (FLAGS_dump_index.size())
            search->set_alloc(make_dump_allocator(search, FLAGS_dump_index));
        else
            search->set_alloc(make_mem_allocator());

        timer tm;
        struct timeval elapsed;
        for (int i = 1; i < argc; i++) {
            walk_one(search, argv[i]);
        }
        if (!FLAGS_json)
            printf("Finalizing...\n");
        search->finalize();
        elapsed = tm.elapsed();
        if (!FLAGS_json)
            printf("repository indexed in %d.%06ds\n",
                   (int)elapsed.tv_sec, (int)elapsed.tv_usec);
    } else {
        search->load_index(FLAGS_load_index);
    }
    if (!FLAGS_json && !FLAGS_load_index.size())
        search->dump_stats();
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
    interact(child->search, r, w);
    fclose(r);
    fclose(w);
    delete child;
    return 0;
}

void listen(code_searcher *search, string path) {
    int server = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server < 0)
        die_errno("socket(AF_UNIX)");

    struct sockaddr_un addr;

    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path.c_str(), path.size());

    if (::bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0)
        die_errno("Unable to bind socket");

    if (listen(server, 4) < 0)
        die_errno("listen()");

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


    if (FLAGS_listen.size())
        listen(&search, FLAGS_listen);
    else
        interact(&search, stdin, stdout);

    return 0;
}
