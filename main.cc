#include "codesearch.h"
#include "smart_git.h"
#include "timer.h"

#include <stdio.h>
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
    git_repository *repo;
    git_repository_open(&repo, ".git");

    code_searcher counter(repo);

    for (int i = 1; i < argc; i++) {
        timer tm;
        struct timeval elapsed;
        printf("Walking %s...", argv[i]);
        fflush(stdout);
        counter.walk_ref(argv[i]);
        elapsed = tm.elapsed();
        printf(" done in %d.%06ds\n",
               (int)elapsed.tv_sec, (int)elapsed.tv_usec);
    }
    counter.dump_stats();
    RE2::Options opts;
    opts.set_never_nl(true);
    opts.set_one_line(false);
    opts.set_perl_classes(true);
    opts.set_posix_syntax(true);
    while (true) {
        printf("regex> ");
        string line;
        getline(cin, line);
        if (cin.eof())
            break;
        RE2 re(line, opts);
        if (re.ok()) {
            timer tm;
            struct timeval elapsed;
            if (!counter.match(re)) {
                printf("no match\n");
            }
            elapsed = tm.elapsed();
            printf("Match completed in %d.%06ds.\n",
                   (int)elapsed.tv_sec, (int)elapsed.tv_usec);
        }
    }

    return 0;
}
