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
#include "src/smart_git.h"

class code_searcher;
class git_repository;
class git_tree;
struct indexed_tree;

// This should be enough to recover a file/bob from a repo
struct pre_indexed_file {
    const indexed_tree *tree;
     std::string  repopath;
     std::string  path;
     std::string id; // string version of oid
     const unsigned char * id_test2;
     int score;
};

class git_indexer {
public:
    git_indexer(code_searcher *cs,
                const google::protobuf::RepeatedPtrField<RepoSpec>& repositories);
    ~git_indexer();
    void walk(const std::string& ref);
    void begin_indexing();
protected:
    void walk_tree(const std::string& pfx,
                   const std::string& order,
                   git_tree *tree);
    void index_files();

    code_searcher *cs_;
    git_repository *repo_;
    const indexed_tree *idx_tree_;
    std::string repopath_;
    std::string name_;
    Metadata metadata_;
    bool walk_submodules_;
    std::string submodule_prefix_;
    const google::protobuf::RepeatedPtrField<RepoSpec>& repositories_to_index_;
    std::vector<std::unique_ptr<pre_indexed_file>> files_to_index_;
};

#endif
