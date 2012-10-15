/********************************************************************
 * livegrep -- radix_sort.h
 * Copyright (c) 2011-2012 Nelson Elhage
 * All Rights Reserved
 ********************************************************************/
#ifndef CODESEARCH_RADIX_SORT_H
#define CODESEARCH_RADIX_SORT_H

#include <algorithm>

const size_t kRadixCutoff = 128;

template <typename Index, typename Compare>
void msd_radix_sort(uint32_t *left, uint32_t *right, int level,
                    Index index, Compare cmp) {
    if (right - left < kRadixCutoff) {
        std::sort(left, right, cmp);
        return;
    }
    unsigned counts[256] = {};
    unsigned dest[256];
    uint32_t *p;
    for (p = left; p != right; p++)
        counts[index(*p, level)]++;
    for (int i = 0, total = 0; i < 256; i++) {
        int tmp = counts[i];
        counts[i] = total;
        total += tmp;
    }
    memcpy(dest, counts, sizeof counts);
    int this_chunk;
    for (p = left, this_chunk = 0; this_chunk < 255;) {
        if (p - left == counts[this_chunk + 1]) {
            this_chunk++;
            continue;
        }
        int target = index(*p, level);
        if (target == this_chunk) {
            p++;
            continue;
        }
        assert(dest[target] < (right - left));
        swap(left[dest[target]++], *p);
    }
    for (int i = 1; i < 256; i++) {
        uint32_t *r = (i == 255) ? right : left + counts[i+1];
        msd_radix_sort(left + counts[i], r, level + 1, index, cmp);
    }
}

void lsd_radix_sort(uint32_t *left, uint32_t *right);

#endif
