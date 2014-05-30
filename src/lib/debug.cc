/********************************************************************
 * livegrep -- debug.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "debug.h"

#include <gflags/gflags.h>

#include <string>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

using std::string;

debug_mode debug_enabled;

DEFINE_string(debug, "", "Enable debugging for selected subsystems");

struct debug_flag {
    const char *flag;
    debug_mode bits;
} debug_flags[] = {
    {"search",    kDebugSearch},
    {"profile",   kDebugProfile},
    {"index",     kDebugIndex},
    {"indexall",  kDebugIndexAll},
    {"indexall",  kDebugIndexAll},
    {"ui",        kDebugUI},
    {"all",       (debug_mode)-1}
};

static bool validate_debug(const char *flagname, const string& value) {
    off_t off = 0;
    while (off < value.size()) {
        string opt;
        off_t comma = value.find(',', off);
        if (comma == string::npos)
            comma = value.size();
        opt = value.substr(off, comma - off);
        off = comma + 1;

        bool found = false;
        for (int i = 0; i < sizeof(debug_flags)/sizeof(*debug_flags); ++i) {
            if (opt == debug_flags[i].flag) {
                found = true;
                debug_enabled = static_cast<debug_mode>(debug_enabled | debug_flags[i].bits);
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    return true;
}

static const bool dummy = google::RegisterFlagValidator(&FLAGS_debug,
                                                        validate_debug);


string vstrprintf(const char *fmt, va_list ap) {
    char *buf = NULL;
    assert(vasprintf(&buf, fmt, ap) > 0);

    string out = buf;
    free(buf);
    return out;
}

string strprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    return vstrprintf(fmt, ap);
    va_end(ap);
}

void cs_debug(const char *file, int lno, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    string buf = strprintf("[%s:%d] %s\n",
                           file, lno, vstrprintf(fmt, ap).c_str());

    va_end(ap);

    fputs(buf.c_str(), stderr);
}


void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}
