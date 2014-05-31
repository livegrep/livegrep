/********************************************************************
 * livegrep -- radix_sorter.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_RADIX_SORTER_H
#define CODESEARCH_RADIX_SORTER_H

#include <algorithm>

using std::min;

class radix_sorter {
public:
    radix_sorter(const unsigned char *data, int size) : data_(data), size_(size) { }

    ~radix_sorter() { }

    void sort(uint32_t *l, uint32_t *r);

    struct cmp_suffix {
        radix_sorter &sort;
        cmp_suffix(radix_sorter &s) : sort(s) {}
        bool operator()(uint32_t lhs, uint32_t rhs) {
            const unsigned char *l = &sort.data_[lhs];
            const unsigned char *r = &sort.data_[rhs];
            unsigned ll = static_cast<const unsigned char*>
                (memchr(l, '\n', sort.size_ - lhs)) - l;
            unsigned rl = static_cast<const unsigned char*>
                (memchr(r, '\n', sort.size_ - rhs)) - r;
            int cmp = memcmp(l, r, min(ll, rl));
            if (cmp < 0)
                return true;
            if (cmp > 0)
                return false;
            return ll < rl;
        }
    };

    struct indexer {
        radix_sorter &sort;
        indexer(radix_sorter &s) : sort(s) {}
        unsigned operator()(uint32_t off, int i) {
            return sort.index(off, i);
        }
    };

private:
    unsigned index(uint32_t off, int i) {
        if (data_[off + i] == '\n')
            return 0;
        return (unsigned)(unsigned char)data_[off + i];
    }

    const unsigned char *data_;
    ssize_t size_;

    radix_sorter(const radix_sorter&);
    radix_sorter operator=(const radix_sorter&);
};

#endif
