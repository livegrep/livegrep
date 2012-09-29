#ifndef CODESEARCH_CHUNK_H
#define CODESEARCH_CHUNK_H

#include <assert.h>
#include <string.h>

#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <list>

#include <stdint.h>

struct search_file;
namespace re2 {
    class StringPiece;
}

using namespace std;
using re2::StringPiece;

/*
 * A chunk_file in a given chunk's `files' list means that some or all
 * of bytes `left' through `right' (inclusive on both sides) in
 * chunk->data are present in each of chunk->files.
 */
struct chunk_file {
    list<search_file *> files;
    int left;
    int right;
    void expand(int l, int r) {
        left  = min(left, l);
        right = max(right, r);
    }

    bool operator<(const chunk_file& rhs) const {
        return left < rhs.left || (left == rhs.left && right < rhs.right);
    }
};

const size_t kMaxGap       = 1 << 10;

struct chunk_file_node {
    chunk_file *chunk;
    int right_limit;

    chunk_file_node *left, *right;
};

struct chunk {
    static int chunk_files;

    int id;     // Sequential id
    int size;
    vector<chunk_file> files;
    vector<chunk_file> cur_file;
    chunk_file_node *cf_root;
    uint32_t *suffixes;
    unsigned char *data;

    chunk(unsigned char *data, uint32_t *suffixes)
        : size(0), files(),
          suffixes(suffixes), data(data) {
    }

    ~chunk() {
    }

    void add_chunk_file(search_file *sf, const StringPiece& line);
    void finish_file();
    void finalize();
    void finalize_files();
    void build_tree();

    struct lt_suffix {
        const chunk *chunk_;
        lt_suffix(const chunk *chunk) : chunk_(chunk) { }
        bool operator()(uint32_t lhs, uint32_t rhs) {
            const unsigned char *l = &chunk_->data[lhs];
            const unsigned char *r = &chunk_->data[rhs];
            const unsigned char *le = static_cast<const unsigned char*>
                (memchr(l, '\n', chunk_->size - lhs));
            const unsigned char *re = static_cast<const unsigned char*>
                (memchr(r, '\n', chunk_->size - rhs));
            assert(le);
            assert(re);
            return memcmp(l, r, min(le - l, re - r)) < 0;
        }

        bool operator()(uint32_t lhs, const string& rhs) {
            return cmp(lhs, rhs) < 0;
        }

        bool operator()(const string& lhs, uint32_t rhs) {
            return cmp(rhs, lhs) > 0;
        }

    private:
        int cmp(uint32_t lhs, const string& rhs) {
            const unsigned char *l = &chunk_->data[lhs];
            const unsigned char *le = static_cast<const unsigned char*>
                (memchr(l, '\n', chunk_->size - lhs));
            size_t lhs_len = le - l;
            int cmp = memcmp(l, rhs.c_str(), min(lhs_len, rhs.size()));
            if (cmp == 0)
                return lhs_len < rhs.size() ? -1 : 0;
            return cmp;
        }
    };

    chunk_file_node *build_tree(int left, int right);

private:
    chunk(const chunk&);
    chunk operator=(const chunk&);
};

extern size_t kChunkSpace;

#endif
