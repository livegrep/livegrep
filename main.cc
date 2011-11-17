#include "codesearch.h"
#include "smart_git.h"
#include "timer.h"

#include <stdio.h>
#include <iostream>

#include <gflags/gflags.h>

#include <json/json.h>

#include <re2/regexp.h>

DEFINE_bool(json, false, "Use JSON output.");
DEFINE_int32(threads, 4, "Number of threads to use.");
DEFINE_string(dump_index, "", "Dump the produced index to a specified file");
DEFINE_string(load_index, "", "Load the index from a file instead of walking the repository");
DEFINE_string(git_dir, ".git", "The git directory to read from");

using namespace std;

long timeval_ms (struct timeval tv) {
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void print_stats(const match_stats &stats) {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "re2_time", json_object_new_int
                           (timeval_ms(stats.re2_time)));
    json_object_object_add(obj, "git_time", json_object_new_int
                           (timeval_ms(stats.git_time)));
    printf("DONE %s\n", json_object_to_json_string(obj));
    json_object_put(obj);
}

int main(int argc, char **argv) {
    google::SetUsageMessage("Usage: " + string(argv[0]) + " <options> REFS");
    google::ParseCommandLineFlags(&argc, &argv, true);

    git_repository *repo;
    git_repository_open(&repo, FLAGS_git_dir.c_str());

    code_searcher counter(repo);
    counter.set_output_json(FLAGS_json);

    if (FLAGS_load_index.size() == 0) {
        timer tm;
        struct timeval elapsed;

        for (int i = 1; i < argc; i++) {
            if (!FLAGS_json)
                printf("Walking %s...", argv[i]);
            fflush(stdout);
            counter.walk_ref(argv[i]);
            elapsed = tm.elapsed();
            if (!FLAGS_json)
                printf(" done.\n");
        }
        counter.finalize();
        elapsed = tm.elapsed();
        if (!FLAGS_json)
            printf("repository indexed in %d.%06ds\n",
                   (int)elapsed.tv_sec, (int)elapsed.tv_usec);
    } else {
        counter.load_index(FLAGS_load_index);
    }
    if (!FLAGS_json && !FLAGS_load_index.size())
        counter.dump_stats();
    if (FLAGS_dump_index.size())
        counter.dump_index(FLAGS_dump_index);
    RE2::Options opts;
    opts.set_never_nl(true);
    opts.set_one_line(false);
    opts.set_perl_classes(true);
    opts.set_posix_syntax(true);
    opts.set_log_errors(false);
    while (true) {
        if (FLAGS_json)
            printf("READY\n");
        else
            printf("regex> ");
        string line;
        getline(cin, line);
        if (cin.eof())
            break;
        RE2 re(line, opts);
        if (!re.ok()) {
            if (!FLAGS_json)
                printf("Error: %s\n", re.error().c_str());
            else
                printf("FATAL %s\n", re.error().c_str());
        }
        if (re.ok()) {
            timer tm;
            struct timeval elapsed;
            match_stats stats;
            counter.match(re, &stats);
            elapsed = tm.elapsed();
            if (FLAGS_json)
                print_stats(stats);
            else
                printf("Match completed in %d.%06ds.\n",
                       (int)elapsed.tv_sec, (int)elapsed.tv_usec);
        }
    }

    return 0;
}
