/********************************************************************
 * livegrep -- chunk.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_CHUNK_H
#define CODESEARCH_CHUNK_H

#include <assert.h>
#include <string.h>

#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <list>
#include <set>
#include <memory>

#include <stdint.h>

struct indexed_file;
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
    list<indexed_file *> files;
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

    std::unique_ptr<chunk_file_node> left, right;
};

struct chunk {
    // total number of chunk_file objects across all chunks.
    static int chunk_files;

    int id;     // Sequential id
    int size;

    // Collects references to all files which contain lines stored in this
    // chunk's data. Sorted (and compacted) at the very end of index creation.
    vector<chunk_file> files;

    // Collects the names of all trees indexed in this chunk, to enable
    // short-circuiting based on a repo constraint.
    set<string> tree_names;

    // Transient during index creation. Collects references to the file
    // currently being processed by the code_searcher, when that file contains
    // lines stored in this chunk's data. One the code_searcher finishes
    // processing each file, any references here are merged into `files` by
    // finish_file(), and this vector is cleared.
    vector<chunk_file> cur_file;

    // BST constructed from `files` at the very end of index creation. Used to
    // efficiently find, given a substring of this chunk's data, the files
    // might contain that substring.
    unique_ptr<chunk_file_node> cf_root;

    // The suffix array; constructed from `data` during finalization (once the
    // chunk's data block is full, but before all files have been processed).
    uint32_t *suffixes;

    // Many lines of code, from many files, concatenated together.
    unsigned char *data;

    chunk(unsigned char *data, uint32_t *suffixes)
        : size(0), files(), cf_root(),
          suffixes(suffixes), data(data) { }

    void add_chunk_file(indexed_file *sf, const StringPiece& line);
    void finish_file();
    void finalize();
    void finalize_files();
    void build_tree_names();
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

    unique_ptr<chunk_file_node> build_tree(int left, int right);

private:
    chunk(const chunk&);
    chunk operator=(const chunk&);
};

extern size_t kChunkSpace;

#endif
