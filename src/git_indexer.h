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
#include "src/proto/config.pb.h"

class code_searcher;
class git_repository;
class git_tree;
struct indexed_tree;

class git_indexer {
public:
    git_indexer(code_searcher *cs,
                const std::string& repopath,
                const std::string& name,
                const Metadata &metadata,
                bool walk_submodules);
    ~git_indexer();
    void walk(const std::string& ref);
protected:
    void walk_tree(const std::string& pfx,
                   const std::string& order,
                   git_tree *tree);

    code_searcher *cs_;
    git_repository *repo_;
    const indexed_tree *idx_tree_;
    std::string repopath_;
    std::string name_;
    Metadata metadata_;
    bool walk_submodules_;
    std::string submodule_prefix_;
};

#endif
