/********************************************************************
 * livegrep -- btree.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/

#include <string.h>
#include <vector>
#include <algorithm>

#include "chunk_allocator.h"
#include "debug.h"

using namespace std;

const uint32_t kBTreeLeaf = 1 << 0;
const uint32_t kBTreeRoot = 1 << 1;

struct lt_suffix {
    const chunk_allocator *alloc_;
    lt_suffix(const chunk_allocator *alloc) : alloc_(alloc) { }

    bool operator()(text_pos lhs, text_pos rhs) {
        const unsigned char *l = alloc_->text_at(lhs);
        const unsigned char *r = alloc_->text_at(rhs);
        unsigned ll = static_cast<const unsigned char*>
            (rawmemchr(l, '\n')) - l;
        unsigned rl = static_cast<const unsigned char*>
            (rawmemchr(r, '\n')) - r;
        int cmp = memcmp(l, r, min(ll, rl));
        if (cmp != 0)
            return cmp < 0;
        if (ll != rl)
            return ll < rl;
        return l < r;
    }

    bool operator()(text_pos lhs, const string& rhs) {
        return cmp(lhs, rhs) < 0;
    }

    bool operator()(const string& lhs, text_pos rhs) {
        return cmp(rhs, lhs) > 0;
    }

private:
    int cmp(text_pos lhs, const string& rhs) {
        const unsigned char *l = alloc_->text_at(lhs);
        const unsigned char *le = static_cast<const unsigned char*>
            (rawmemchr(l, '\n'));
        size_t lhs_len = le - l;
        int cmp = memcmp(l, rhs.c_str(), min(lhs_len, rhs.size()));
        if (cmp == 0)
            return lhs_len < rhs.size() ? -1 : 0;
        return cmp;
    }
};

void chunk_allocator::prepare_root(btree_node *node) {
    node->flags = kBTreeRoot | kBTreeLeaf;
}

static int search(lt_suffix& lt, btree_node *node, text_pos pos) {
    if (node->count == 0 || lt(pos, node->entries[0]))
        return 0;
    return lower_bound(node->entries, node->entries + node->count, pos, lt) - node->entries;
}

void chunk_allocator::btree_check_invariants(btree_node *node) {
    if (!(debug_enabled & kDebugBTree))
        return;
    lt_suffix lt(this);
    assert(node->count <= kBTreeEntries);
    for (int i = 0; i < node->count; i++) {
        assert(node->entries[i].pos);
        if (i != node->count - 1)
            assert(lt(node->entries[i], node->entries[i+1]));
        if (!(node->flags & kBTreeLeaf)) {
            assert(lt(node_at(node->children[i])->entries[0], node->entries[i]));
            assert(lt(node->entries[i], node_at(node->children[i+1])->entries[0]));
        } else {
            assert(node->children[i]   == kNullPos);
            assert(node->children[i+1] == kNullPos);
        }
    }
}

void chunk_allocator::btree_insert(text_pos pos) {
    lt_suffix lt(this);

    vector<btree_node *> stack;
    btree_node *node = btree_root();

    while (!(node->flags & kBTreeLeaf)) {
        stack.push_back(node);
        int at = search(lt, node, pos);
        node = node_at(node->children[at]);
    }

    struct {
        text_pos text;
        btree_pos left;
        btree_pos right;
    } insert = {
        .text = pos
    };

    while (node->count == kBTreeEntries) {
        // Node is full. Split into two halves
        btree_check_invariants(node);

        text_pos median = node->entries[kBTreeEntries/2];
        btree_node *right = alloc_node();
        btree_node *left  = node;

        btree_node tmp = *node;

        if (node->flags & kBTreeLeaf)
            right->flags |= kBTreeLeaf;

        int epos = 0, cpos = 0;

        int pos = search(lt, node, insert.text);
        int i;

        cpos = epos = 0;
        for (i = 0; i < kBTreeEntries/2; ++i) {
            left->children[cpos++] = tmp.children[i];
            if (i == pos) {
                assert(insert.left == tmp.children[i]);
                left->entries[epos++] = insert.text;
                left->children[cpos++] = insert.right;
            }
            left->entries[epos++] = tmp.entries[i];
        }
        left->children[cpos++] = tmp.children[i];
        left->count = epos;

        cpos = epos = 0;
        for (i = kBTreeEntries/2 + 1; i < kBTreeEntries; ++i) {
            right->children[cpos++] = tmp.children[i];
            if (i == pos) {
                assert(insert.left == tmp.children[i]);
                right->entries[epos++] = insert.text;
                right->children[cpos++] = insert.right;
            }
            right->entries[epos++] = tmp.entries[i];
        }
        right->children[cpos++] = tmp.children[i];
        right->count = epos;

        btree_check_invariants(left);
        btree_check_invariants(right);

        insert = {
            .text = median,
            .left = btree_to_pos(left),
            .right = btree_to_pos(right)
        };
        assert(insert.left == kNullPos || lt(node_at(insert.left)->entries[0], insert.text));
        assert(insert.right == kNullPos || lt(insert.text, node_at(insert.right)->entries[0]));

        if (stack.empty()) {
            node = 0;
            break;
        } else {
            node = stack.back();
            stack.pop_back();
        }
    }

    if (node) {
        int pos = search(lt, node, insert.text);
        assert(node->children[pos] == insert.left);
        memmove(node->entries + pos + 1, node->entries + pos, (node->count - pos) * sizeof(*node->entries));
        memmove(node->children + pos + 2, node->children + pos + 1, (node->count - pos) * sizeof(*node->children));
        node->entries[pos] = insert.text;
        node->children[pos + 1] = insert.right;
        node->count++;
        btree_check_invariants(node);
    } else {
        assert(node_at(insert.left)->flags & kBTreeRoot);
        node_at(insert.left)->flags &= ~kBTreeRoot;

        node = alloc_node();
        node->flags |= kBTreeRoot;
        node->count = 1;
        node->entries[0]  = insert.text;
        node->children[0] = insert.left;
        node->children[1] = insert.right;
        btree_check_invariants(node);
        btree_set_root(node);
    }
}
