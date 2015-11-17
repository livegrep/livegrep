#include <stdlib.h>
#include <string.h>

#include <string>

#include "dump_load.h"
#include "codesearch.h"
#include "content.h"
#include "debug.h"
#include "indexer.h"
#include "re_width.h"

#include <gflags/gflags.h>

void dump_file(code_searcher *cs, indexed_file *f) {
    for (auto it = f->content->begin(cs->alloc());
         it != f->content->end(cs->alloc()); ++it) {
        printf("%.*s\n", it->size(), it->data());
    }
}

int dump_file(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <options> INDEX PATH\n", google::GetArgv0());
        return 1;
    }

    string index = argv[0];
    string path = argv[1];

    code_searcher cs;
    cs.load_index(index);

    for (auto it = cs.begin_files(); it != cs.end_files(); ++it) {
        if ((*it)->path == path) {
            dump_file(&cs, *it);
            return 0;
        }
    }

    fprintf(stderr, "No files matching path: %s\n", path.c_str());

    return 0;
}
