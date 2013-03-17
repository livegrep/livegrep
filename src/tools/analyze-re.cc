#include <gflags/gflags.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <algorithm>

#include <string>

#include "dump_load.h"
#include "codesearch.h"
#include "debug.h"
#include "indexer.h"
#include "re_width.h"

int main(int argc, char **argv) {
    google::SetUsageMessage("Usage: " + string(argv[0]) + " <options> REGEX");
    google::ParseCommandLineFlags(&argc, &argv, true);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <options> REGEX\n", argv[0]);
        return 1;
    }

    RE2::Options opts;
    default_re2_options(opts);

    RE2 re(argv[1], opts);
    if (!re.ok()) {
        fprintf(stderr, "Error: %s\n", re.error().c_str());
        return 1;
    }

    WidthWalker width;
    printf("== RE [%s] ==\n", argv[1]);
    printf("width: %d\n", width.Walk(re.Regexp(), 0));
    printf("Program size: %d\n", re.ProgramSize());

    intrusive_ptr<IndexKey> key = indexRE(re);
    if (key) {
        IndexKey::Stats stats = key->stats();
        printf("Index key:\n");
        printf("  log10(selectivity): %f\n", log(stats.selectivity_)/log(10));
        printf("  depth: %d\n", stats.depth_);
        printf("  nodes: %ld\n", stats.nodes_);
    } else {
        printf("(Unindexable)\n");
    }

    return 0;
}
