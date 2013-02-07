/********************************************************************
 * livegrep -- radix_sort.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include <stdint.h>
#include <string.h>
#include <vector>

using std::vector;

#include "per_thread.h"

void lsd_radix_sort(uint32_t *left, uint32_t *right)
{
    static per_thread<vector<uint32_t> > scratch;

    int width = right - left;
    if (!scratch.get())
        scratch.put(new vector<uint32_t>(width));
    scratch->reserve(width);
    uint32_t *cur = left, *other = &(*scratch)[0];
    uint32_t counts[4][256];
    /*
     * We do four passes
     * (0) input -> scratch
     * (1) scratch -> input
     * (2) input -> scratch
     * (3) scratch -> input
     *
     * So after the fourth pass, the input array is sorted and back in
     * the original storage.
     */

    memset(counts, 0, sizeof counts);
    for (int i = 0; i < width; i++) {
        counts[0][(cur[i] >> 0 ) & 0xFF]++;
        counts[1][(cur[i] >> 8 ) & 0xFF]++;
        counts[2][(cur[i] >> 16) & 0xFF]++;
        counts[3][(cur[i] >> 24) & 0xFF]++;
    }

    for (int digit = 0; digit < 4; digit++) {
        int total = 0;
        for (int i = 0; i < 256; i++) {
            int tmp = counts[digit][i];
            counts[digit][i] = total;
            total += tmp;
        }
        for (int i = 0; i < width; i++) {
            int d = (cur[i] >> (8 * digit)) & 0xFF;
            other[counts[digit][d]++] = cur[i];
        }
        uint32_t *tmp = cur;
        cur = other;
        other = tmp;
    }
}
