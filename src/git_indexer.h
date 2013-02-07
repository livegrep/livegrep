/********************************************************************
 * livegrep -- git_indexer.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_GIT_INDEXER_H
#define CODESEARCH_GIT_INDEXER_H

#include <string>

class code_searcher;
class git_repository;
class git_tree;

class git_indexer {
public:
    git_indexer(code_searcher *cs,
                const std::string& repopath,
                const std::string& name);
    ~git_indexer();
    void walk(const std::string& ref);
protected:
    void walk_root(const std::string& ref, git_tree *tree);
    void walk_tree(const std::string& ref,
                   const std::string& pfx, git_tree *tree);

    code_searcher *cs_;
    git_repository *repo_;
    std::string name_;
};

#endif
