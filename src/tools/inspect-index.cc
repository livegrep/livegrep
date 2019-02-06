#include <stdint.h>
#include <fstream>
#include <fcntl.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <algorithm>

#include <string>

#include "src/lib/debug.h"

#include "src/dump_load.h"
#include "src/codesearch.h"

#include <gflags/gflags.h>

using std::string;

struct index_span {
    unsigned long left;
    unsigned long right;
    string name;

    index_span(unsigned long l, unsigned long r, const string& name)
        : left(l), right(r), name(name) { }
};

bool operator<(const index_span& left, const index_span& right) {
    return left.left < right.left;
}

DEFINE_bool(dump_spans, false, "Dump detailed index span information.");
DEFINE_bool(dump_trees, false, "Dump tree names.");
DEFINE_string(dump_source, "", "Dump full indexed source to file.");

int inspect_index(int argc, char **argv) {
    if (argc != 1) {
        fprintf(stderr, "Usage: %s <options> INDEX.idx\n", gflags::GetArgv0());
        return 1;
    }

    int fd;
    struct stat st;
    uint8_t *map;

    vector<index_span> spans;

    fd = open(argv[0], O_RDONLY);
    if (fd <= 0) {
        die("open('%s'): %s\n", argv[0], strerror(errno));
    }
    int err = fstat(fd, &st);
    if (err != 0) {
        die("fstat: %s\n", strerror(errno));
    }
    map = static_cast<uint8_t*>(mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0));
    assert(map != MAP_FAILED);

    index_header *idx = reinterpret_cast<index_header*>(map);

    if (idx->magic != kIndexMagic) {
        fprintf(stderr, "Bad Magic: %x\n", idx->magic);
        return 1;
    }

    if (idx->version != kIndexVersion) {
        fprintf(stderr, "Bad version number: %d (we support: %d)\n",
                idx->version, kIndexVersion);
        return 1;
    }

    spans.push_back(index_span(0, sizeof(index_header), "global header"));

    printf("Index: %s\n", argv[1]);
    printf("Name: %.*s\n",
           *reinterpret_cast<uint32_t*>(map + idx->name_off),
           map + idx->name_off + sizeof(uint32_t));
    printf(" Chunk size: %d ", idx->chunk_size);
    if ((idx->chunk_size & (idx->chunk_size - 1)) == 0) {
        printf("(1 << %d)\n", ffsl(idx->chunk_size) - 1);
    } else {
        printf("(not a power of two?)\n");
    }
    printf(" Trees: %d\n", idx->ntrees);
    printf(" Files: %d\n", idx->nfiles);
    printf(" File size: %ld (%0.2fM)\n", st.st_size, st.st_size / double(1 << 20));
    printf(" Chunks: %d (%dM) (%dM indexes)\n", idx->nchunks,
           (idx->nchunks * idx->chunk_size) >> 20,
           (idx->nchunks * idx->chunk_size) >> 18);
    unsigned long content_size = 0;
    content_chunk_header *chdrs = reinterpret_cast<content_chunk_header*>
        (map + idx->content_off);
    spans.push_back(index_span(idx->content_off,
                idx->content_off + idx->ncontent * sizeof(content_chunk_header),
                               "content chunk headers"));
    for (int i = 0; i < idx->ncontent; i++) {
        content_size += (chdrs[i].size + ((1<<20) - 1)) & ~((1<<20) - 1);
        spans.push_back(index_span(chdrs[i].file_off,
                    chdrs[i].file_off + chdrs[i].size,
                                   strprintf("content chunk %d", i)));
    }
    printf(" Content chunks: %d (%ldM)\n",
           idx->ncontent, content_size >> 20);
    uint8_t *p = map + idx->files_off;
    for (int i = 0; i < idx->nfiles; i++) {
        p += 4;
        p += 4 + *reinterpret_cast<uint32_t*>(p);
    }
    spans.push_back(index_span(idx->files_off,
                               (unsigned long)(p - map),
                               "file list" ));

    printf(" Filename data: %ld (%0.2fM)\n",
           (p - (map + idx->files_off)),
           (p - (map + idx->files_off))/double(1<<20));

    unsigned long chunk_file_size = 0;
    chunk_header *chunks = reinterpret_cast<chunk_header*>
        (map + idx->chunks_off);
    spans.push_back(index_span(idx->chunks_off,
                               idx->chunks_off + idx->nchunks * sizeof(chunk_header),
                               "chunk headers" ));
    for (int i = 0; i < idx->nchunks; i++) {
        spans.push_back(index_span(chunks[i].data_off,
                                   chunks[i].data_off + idx->chunk_size,
                                   strprintf("chunk %d", i)));
        spans.push_back(index_span(chunks[i].data_off + idx->chunk_size,
                                   chunks[i].data_off +
                                   (1 + sizeof(uint32_t)) * idx->chunk_size,
                                   strprintf("chunk %d indexes", i)));
        p = map + chunks[i].files_off;
        for (int j = 0; j < chunks[i].nfiles; ++j) {
            uint32_t files = *reinterpret_cast<uint32_t*>(p);
            p += 4;
            p += files * 4;
            p += 8;
        }
        chunk_file_size += p - (map + chunks[i].files_off);
        spans.push_back(index_span(chunks[i].files_off,
                                   (unsigned long)(p - map),
                                   strprintf("chunk %d file map", i)));
    }
    printf(" chunk_file data: %ld (%0.2fM)\n",
           chunk_file_size,
           chunk_file_size / double(1 << 20));

    code_searcher cs;
    if (FLAGS_dump_trees) {
        cs.load_index(argv[0]);
        auto trees = cs.trees();
        printf("Trees:\n");
        for (auto it = trees.begin(); it != trees.end(); ++it) {
            printf(" %s%s%s\n",
                   it->name.c_str(),
                   it->version.empty() ? "" : ":",
                   it->version.c_str());
        }
    }

    if (FLAGS_dump_spans) {
        printf("Span table:\n");
        sort(spans.begin(), spans.end());
        unsigned long prev = 0;
        for (auto it = spans.begin(); it != spans.end(); ++it) {
            assert(prev <= it->left);
            assert(it->left < it->right);
            printf("%016lx-%016lx %s\n", it->left, it->right, it->name.c_str());
            prev = it->right;
        }
    }

    if (FLAGS_dump_source.size()) {
        std::ofstream dump(FLAGS_dump_source, std::ios::trunc);
        if (dump.bad()) {
            die("open(%s): %s", FLAGS_dump_source.c_str(), strerror(errno));
        }
        for (int i = 0; i < idx->nchunks; i++) {
            auto *data = reinterpret_cast<const char*>(map + chunks[i].data_off);
            auto *end = reinterpret_cast<const char*>(map + chunks[i].data_off + chunks[i].size);
            dump.write(data, end-data);
            dump << '\n';
        }
    }

    return 0;
}
