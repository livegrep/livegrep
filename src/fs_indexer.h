/********************************************************************
 * livegrep -- fs_indexer.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_FS_INDEXER_H
#define CODESEARCH_FS_INDEXER_H

#include <string>

class code_searcher;

class fs_indexer {
public:
    fs_indexer(code_searcher *cs,
                             const string& name);
    ~fs_indexer();
    void read_file(const std::string& path);
    void walk(const std::string& path);
protected:
    code_searcher *cs_;
    std::string name_;
};

#endif
