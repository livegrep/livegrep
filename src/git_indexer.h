/********************************************************************
 * livegrep -- git_indexer.h
 * Copyright (c) 2011-2012 Nelson Elhage
 * All Rights Reserved
 ********************************************************************/
#ifndef CODESEARCH_GIT_INDEXER_H
#define CODESEARCH_GIT_INDEXER_H

#include <string>

class code_searcher;
class git_repository;
class git_tree;

class git_indexer {
public:
    git_indexer(code_searcher *cs, const std::string& repopath);
    ~git_indexer();
    void walk(const std::string& ref);
protected:
    void walk_root(const std::string& ref, git_tree *tree);
    void walk_tree(const std::string& ref,
                   const std::string& pfx, git_tree *tree);

    code_searcher *cs_;
    git_repository *repo_;
};

#endif
