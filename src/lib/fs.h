/********************************************************************
 * livegrep -- fs.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_FS_H
#define CODESEARCH_FS_H

#include <string>

using namespace std;

class fswatcher {
public:
    fswatcher(const std::string &path);
    ~fswatcher();

    bool wait_for_event();

private:
    std::string path_;
};

#endif
