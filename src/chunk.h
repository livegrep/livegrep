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

    chunk_file_node *left, *right;
};

class chunk_allocator;

struct chunk {
    static int chunk_files;

    int id;     // Sequential id
    int size;
    vector<chunk_file> files;
    vector<chunk_file> cur_file;
    chunk_file_node *cf_root;
    unsigned char *data;

    chunk(unsigned char *data)
        : size(0), files(), cf_root(0),
          data(data) { }

    ~chunk() {
    }

    void add_chunk_file(indexed_file *sf, const StringPiece& line);
    void finish_file();
    void finalize(chunk_allocator *alloc);
    void finalize_files();
    void build_tree();

    chunk_file_node *build_tree(int left, int right);

private:
    chunk(const chunk&);
    chunk operator=(const chunk&);
};

extern size_t kChunkSpace;

#endif
