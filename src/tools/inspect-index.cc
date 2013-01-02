#include <gflags/gflags.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <string>

#include "dump_load.h"
#include "codesearch.h"

using std::string;

int main(int argc, char **argv) {
    google::SetUsageMessage("Usage: " + string(argv[0]) + " <options> INDEX.idx");
    google::ParseCommandLineFlags(&argc, &argv, true);

    if (argc < 1) {
        fprintf(stderr, "Usage: %s <options> INDEX.idx\n", argv[0]);
        return 1;
    }

    int fd;
    struct stat st;
    uint8_t *map;

    fd = open(argv[1], O_RDONLY);
    assert(fd > 0);
    assert(fstat(fd, &st) == 0);
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

    printf("Index: %s\n", argv[1]);
    printf(" Chunk size: %d ", idx->chunk_size);
    if ((idx->chunk_size & (idx->chunk_size - 1)) == 0) {
        printf("(1 << %d)\n", ffsl(idx->chunk_size) - 1);
    } else {
        printf("(not a power of two?)\n");
    }
    printf(" Trees: %d\n", idx->ntrees);
    printf(" Files: %d\n", idx->nfiles);
    printf(" File size: %ld (%0.2fM)\n", st.st_size, st.st_size / double(1 << 20));
    printf(" Chunks: %d (%dM)\n", idx->nchunks,
           (idx->nchunks * idx->chunk_size) >> 20);
    unsigned long content_size = 0;
    content_chunk_header *chdrs = reinterpret_cast<content_chunk_header*>
        (map + idx->content_off);
    for (int i = 0; i < idx->ncontent; i++) {
        content_size += (chdrs[i].size + ((1<<20) - 1)) & ~((1<<20) - 1);
    }
    printf(" Content chunks: %d (%ldM)\n",
           idx->ncontent, content_size >> 20);
    uint8_t *p = map + idx->files_off;
    for (int i = 0; i < idx->nfiles; i++) {
        p += sizeof(sha1_buf);
        p += 4;
        p += strlen(reinterpret_cast<char*>(p));
    }
    printf(" Filename data: %ld (%0.2fM)\n",
           (p - (map + idx->files_off)),
           (p - (map + idx->files_off))/double(1<<20));

    unsigned long chunk_file_size = 0;
    chunk_header *chunks = reinterpret_cast<chunk_header*>
        (map + idx->chunks_off);
    for (int i = 0; i < idx->nchunks; i++) {
        p = map + chunks[i].files_off;
        p += 4;
        p += 4 * chunks[i].nfiles;
        p += 8;
        chunk_file_size += p - (map + chunks[i].files_off);
    }
    printf(" chunk_file data: %ld (%0.2fM)\n",
           chunk_file_size,
           chunk_file_size / double(1 << 20));

    return 0;
}
