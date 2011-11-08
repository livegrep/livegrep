#include "codesearch.h"
#include "smart_git.h"
#include "timer.h"

#include <stdio.h>
#include <iostream>

#include <gflags/gflags.h>

DEFINE_bool(json, false, "Use JSON output.");

using namespace std;

int main(int argc, char **argv) {
    google::SetUsageMessage("Usage: " + string(argv[0]) + " <options> REFS");
    google::ParseCommandLineFlags(&argc, &argv, true);

    git_repository *repo;
    git_repository_open(&repo, ".git");

    code_searcher counter(repo);
    counter.set_output_json(FLAGS_json);

    for (int i = 1; i < argc; i++) {
        timer tm;
        struct timeval elapsed;
        if (!FLAGS_json)
            printf("Walking %s...", argv[i]);
        fflush(stdout);
        counter.walk_ref(argv[i]);
        elapsed = tm.elapsed();
        if (!FLAGS_json)
            printf(" done in %d.%06ds\n",
                   (int)elapsed.tv_sec, (int)elapsed.tv_usec);
    }
    counter.finalize();
    if (!FLAGS_json)
        counter.dump_stats();
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
            counter.match(re);
            elapsed = tm.elapsed();
            if (FLAGS_json)
                printf("DONE\n");
            else
                printf("Match completed in %d.%06ds.\n",
                       (int)elapsed.tv_sec, (int)elapsed.tv_usec);
        }
    }

    return 0;
}
