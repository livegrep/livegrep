/********************************************************************
 * livegrep -- radix_sort.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_RADIX_SORT_H
#define CODESEARCH_RADIX_SORT_H

#include <algorithm>
#include <cstdint>

void lsd_radix_sort(uint32_t *left, uint32_t *right);

#endif
