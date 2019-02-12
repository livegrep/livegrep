#include <stdlib.h>
#include <string.h>

#include <string>

#include "src/lib/debug.h"

#include "src/dump_load.h"
#include "src/codesearch.h"
#include "src/content.h"
#include "src/re_width.h"

#include <gflags/gflags.h>

void dump_file(code_searcher *cs, indexed_file *f) {
    for (auto it = f->content->begin(cs->alloc());
         it != f->content->end(cs->alloc()); ++it) {
        printf("%.*s\n", (int)it->size(), it->data());
    }
}

int dump_file(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <options> INDEX PATH\n", gflags::GetArgv0());
        return 1;
    }

    string index = argv[0];
    string path = argv[1];

    code_searcher cs;
    cs.load_index(index);

    for (auto it = cs.begin_files(); it != cs.end_files(); ++it) {
        if ((*it)->path == path) {
            dump_file(&cs, it->get());
            return 0;
        }
    }

    fprintf(stderr, "No files matching path: %s\n", path.c_str());

    return 0;
}
