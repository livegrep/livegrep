#ifndef CODESEARCH_DEBUG_H
#define CODESEARCH_DEBUG_H

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

#endif
