/********************************************************************
 * livegrep -- chunk.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "src/lib/radix_sort.h"
#include "src/lib/metrics.h"

#include "src/chunk.h"
#include "src/codesearch.h"

#include "divsufsort.h"
#include "re2/re2.h"
#include <gflags/gflags.h>

#include <limits>

using re2::StringPiece;

DECLARE_bool(index);

void chunk::add_chunk_file(indexed_file *sf, const StringPiece& line)
{
    int l = (unsigned char*)line.data() - data;
    int r = l + line.size();
    chunk_file *f = NULL;
    int min_dist = numeric_limits<int>::max(), dist;
    for (vector<chunk_file>::iterator it = cur_file.begin();
         it != cur_file.end(); it ++) {
        if (l <= it->left)
            dist = max(0, it->left - r);
        else if (r >= it->right)
            dist = max(0, l - it->right);
        else
            dist = 0;
        assert(dist == 0 || r < it->left || l > it->right);
        if (dist < min_dist) {
            min_dist = dist;
            f = &(*it);
        }
    }
    if (f && min_dist < kMaxGap) {
        f->expand(l, r);
        return;
    }
    chunk_files++;
    cur_file.push_back(chunk_file());
    chunk_file& cf = cur_file.back();
    cf.files.push_front(sf);
    cf.left = l;
    cf.right = r;
}

void chunk::finish_file() {
    int right = -1;
    sort(cur_file.begin(), cur_file.end());
    for (vector<chunk_file>::iterator it = cur_file.begin();
         it != cur_file.end(); it ++) {
        assert(right < it->left);
        right = max(right, it->right);
    }
    files.insert(files.end(), cur_file.begin(), cur_file.end());
    cur_file.clear();
}

int chunk::chunk_files = 0;

void chunk::finalize() {
    if (FLAGS_index) {
        // For the purposes of livegrep's line-based sorting, we need
        // to sort \n before all other characters. divsufsort,
        // understandably, just lexically-sorts sorts thing. Kludge
        // around by munging the data in-place before and after the
        // sort. Sorting must look at all the data anyways, so this is
        // not an overly-expensive job.
        std::replace(data, data + size, '\n', '\0');
        divsufsort(data, reinterpret_cast<saidx_t*>(suffixes), size);
        std::replace(data, data + size, '\0', '\n');
    }
}

void chunk::finalize_files() {
    sort(files.begin(), files.end());

    vector<chunk_file>::iterator out, in;
    out = in = files.begin();
    while (in != files.end()) {
        *out = *in;
        ++in;
        while (in != files.end() &&
               out->left == in->left &&
               out->right == in->right) {
            out->files.push_back(in->files.front());
            ++in;
        }
        ++out;
    }
    files.resize(out - files.begin());

    build_tree_names();
    build_tree();
}

void chunk::build_tree_names() {
    for (auto it = files.begin(); it != files.end(); it++) {
        for (auto it2 = it->files.begin(); it2 != it->files.end(); it2++) {
            tree_names.insert((*it2)->tree->name);
        }
    }
}

void chunk::build_tree() {
    assert(is_sorted(files.begin(), files.end()));
    cf_root = build_tree(0, files.size());
}

unique_ptr<chunk_file_node> chunk::build_tree(int left, int right) {
    if (right == left)
        return 0;
    int mid = (left + right) / 2;
    auto node = std::make_unique<chunk_file_node>();

    node->chunk = &files[mid];
    node->left  = build_tree(left, mid);
    node->right = build_tree(mid + 1, right);
    node->right_limit = node->chunk->right;
    if (node->left && node->left->right_limit > node->right_limit)
        node->right_limit = node->left->right_limit;
    if (node->right && node->right->right_limit > node->right_limit)
        node->right_limit = node->right->right_limit;
    assert(!node->left  || *(node->left->chunk) < *(node->chunk));
    assert(!node->right || *(node->chunk) < *(node->right->chunk));
    return node;
}
