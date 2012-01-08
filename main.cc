#include "codesearch.h"
#include "smart_git.h"
#include "timer.h"
#include "re_width.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <iostream>

#include <gflags/gflags.h>

#include <json/json.h>

#include <re2/regexp.h>
#include "re2/walker-inl.h"

DEFINE_bool(json, false, "Use JSON output.");
DEFINE_int32(threads, 4, "Number of threads to use.");
DEFINE_string(dump_index, "", "Dump the produced index to a specified file");
DEFINE_string(load_index, "", "Load the index from a file instead of walking the repository");
DEFINE_string(git_dir, ".git", "The git directory to read from");
DEFINE_bool(quiet, false, "Do the search, but don't print results.");
DEFINE_string(listen, "", "Listen on a UNIX socket for connections");

using namespace std;
using namespace re2;

void die_errno(const char *str) {
    perror(str);
    exit(1);
}

long timeval_ms (struct timeval tv) {
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static json_object *to_json(vector<string> vec) {
    json_object *out = json_object_new_array();
    for (vector<string>::iterator it = vec.begin(); it != vec.end(); it++)
        json_object_array_add(out, json_object_new_string(it->c_str()));
    return out;
}

struct print_match {
    print_match(FILE *out) : out_(out) {}

    void print(const match_result *m) const {
        fprintf(out_,
                "%s:%s:%d:%d-%d: %.*s\n",
                m->file->ref,
                m->file->path.c_str(),
                m->lno,
                m->matchleft, m->matchright,
                m->line.size(), m->line.data());
    }

    void print_json(const match_result *m) const {
        json_object *obj = json_object_new_object();
        json_object_object_add(obj, "ref",  json_object_new_string(m->file->ref));
        json_object_object_add(obj, "file", json_object_new_string(m->file->path.c_str()));
        json_object_object_add(obj, "lno",  json_object_new_int(m->lno));
        json_object *bounds = json_object_new_array();
        json_object_array_add(bounds, json_object_new_int(m->matchleft));
        json_object_array_add(bounds, json_object_new_int(m->matchright));
        json_object_object_add(obj, "bounds", bounds);
        json_object_object_add(obj, "line",
                               json_object_new_string_len(m->line.data(),
                                                          m->line.size()));
        json_object_object_add(obj, "context_before",
                               to_json(m->context_before));
        json_object_object_add(obj, "context_after",
                               to_json(m->context_after));
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

void print_stats(FILE *out, const match_stats &stats, exit_reason why) {
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
    switch(why) {
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

void getline(FILE *stream, string &out) {
    char *line = 0;
    size_t n = 0;
    n = getline(&line, &n, stream);
    if (n == 0 || n == (size_t)-1)
        out.clear();
    else
        out.assign(line, n - 1);
}

void interact(code_searcher *cs, FILE *in, FILE *out) {
    code_searcher::search_thread search(cs);
    WidthWalker width;

    setvbuf(in, NULL, _IOLBF, 0);
    setvbuf(out, NULL, _IOLBF, 0);

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
        string line;
        getline(in, line);
        if (feof(in))
            break;
        RE2 re(line, opts);
        if (!re.ok()) {
            print_error(out, re.error());
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
            exit_reason why;

            if (!FLAGS_json)
                fprintf(out, "ProgramSize: %d\n", re.ProgramSize());

            search.match(re, print_match(out), &stats, &why);
            elapsed = tm.elapsed();
            if (FLAGS_json)
                print_stats(out, stats, why);
            else {
                fprintf(out,
                        "Match completed in %d.%06ds.",
                        (int)elapsed.tv_sec, (int)elapsed.tv_usec);
                switch (why) {
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

void initialize_search(code_searcher *search, int argc, char **argv) {
    if (FLAGS_load_index.size() == 0) {
        git_repository *repo;
        git_repository_open(&repo, FLAGS_git_dir.c_str());

        timer tm;
        struct timeval elapsed;

        for (int i = 1; i < argc; i++) {
            if (!FLAGS_json)
                printf("Walking %s...", argv[i]);
            fflush(stdout);
            search->walk_ref(repo, argv[i]);
            elapsed = tm.elapsed();
            if (!FLAGS_json)
                printf(" done.\n");
        }
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
    if (FLAGS_dump_index.size())
        search->dump_index(FLAGS_dump_index);
}

struct child_state {
    int fd;
    code_searcher *search;
};

void *handle_client(void *data) {
    child_state *child = static_cast<child_state*>(data);
    FILE *client = fdopen(child->fd, "w+");
    interact(child->search, client, client);
    close(child->fd);
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

    code_searcher counter;

    initialize_search(&counter, argc, argv);

    if (FLAGS_listen.size())
        listen(&counter, FLAGS_listen);
    else
        interact(&counter, stdin, stdout);

    return 0;
}
