/********************************************************************
 * livegrep -- fs_default.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "fs.h"

#include <future>

fswatcher::fswatcher(const std::string &path) : path_(path) {}

fswatcher::~fswatcher() {}

void fswatcher::wait_for_event() {
    fprintf(stderr, "Error: fswatcher is not supported on this platform\n");

    // Block forever.
    std::promise<void>().get_future().wait();
}
