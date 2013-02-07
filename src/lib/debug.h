/********************************************************************
 * livegrep -- debug.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_DEBUG_H
#define CODESEARCH_DEBUG_H

#include <string>

enum debug_mode {
    kDebugSearch        = 0x0001,
    kDebugProfile       = 0x0002,
    kDebugIndex         = 0x0004,
    kDebugIndexAll      = 0x0008,
};

extern debug_mode debug_enabled;

#define debug(which, ...) do {                          \
    if (debug_enabled & (which))                        \
        cs_debug(__FILE__, __LINE__, ##__VA_ARGS__);    \
    } while (0)                                         \

void cs_debug(const char *file, int lno, const char *fmt, ...)
    __attribute__((format (printf, 3, 4)));

std::string strprintf(const char *fmt, ...)
    __attribute__((format (printf, 1, 2)));

#endif
