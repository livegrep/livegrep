/********************************************************************
 * livegrep -- rlimits.cc
 * Copyright (c) 2022 Rodrigo Silva Mendoza
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "rlimits.h"

#include <stdio.h>
#include <sys/resource.h>
#include <sys/types.h>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

// When indexing repos, in multithreaded mode we may bump up against most soft
// limits. Especially on macOS, which has an insanely low soft limit of 256.
// To work around this, we increase the current processes (codesearch's) soft limit 
// up to the system/user defined hard limit. This may be wasteful, and in the
// future we may want to cap this at say, 10000 or something.
void increaseOpenFileLimitToMax() {
    struct rlimit l;

    if (getrlimit(RLIMIT_NOFILE, &l) != 0) {
        fprintf(stderr, "failed to get RLIMIT_NOFILE. Can't bump max open file descriptors.\n");
        return;
    }

#if defined(__APPLE__)
    // macOS getrlimit sometimes lies, telling us the max is RLIM_INFINITY. In
    // that case, we reach out to sysctlbyname, which gives an accurate max.

    if (l.rlim_max == RLIM_INFINITY) {
        uint32_t real_limit;
        size_t len = sizeof(real_limit);
        if (sysctlbyname("kern.maxfilesperproc", &real_limit, &len, nullptr, 0) != 0) {
            fprintf(stderr, "failed to get sysctlbyname('kern.maxfilesperproc'). Can't bump open file descriptors\n");
            return;
        }

        l.rlim_max = real_limit;
    }
#endif

    if (l.rlim_cur >= l.rlim_max) {
        return;
    }

    fprintf(stderr, "raising process file descriptors from %lu to %lu\n", l.rlim_cur, l.rlim_max);
    l.rlim_cur = l.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &l) != 0) {
        fprintf(stderr, "failed to bump file descriptors\n");
    }
}
