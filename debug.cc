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


static string vstrprintf(const char *fmt, va_list ap) {
    char *buf = NULL;
    assert(vasprintf(&buf, fmt, ap) > 0);

    string out = buf;
    free(buf);
    return out;
}

static string strprintf(const char *fmt, ...)
    __attribute__((format (printf, 1, 2)));

static string strprintf(const char *fmt, ...) {
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
