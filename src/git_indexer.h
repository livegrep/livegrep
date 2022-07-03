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
/* #include "src/lib/per_thread.h" */
#include "src/lib/threadsafe_progress_indicator.h"
#include "src/lib/thread_queue.h"

class code_searcher;
class git_repository;
class git_tree;
struct indexed_tree;

// This should be enough to recover a file/bob from a repo
struct pre_indexed_file {
    const indexed_tree *tree;
    std::string  repopath;
    std::string  path;
    git_oid *oid;
    int score;
    git_repository *repo;
};

// used to thread directory walking
struct tree_to_walk {
    int id;
    std::string prefix;
    std::string order;
    std::string repopath;
    bool walk_submodules;
    string submodule_prefix;
    const indexed_tree *idx_tree;
    git_tree *tree;
    git_repository *repo;
    /* std::vector<std::unique_ptr<pre_indexed_file>>& results; */
};

struct dummy {
    std::string name;
    std::string repopath;
};
class git_indexer {
public:
    git_indexer(code_searcher *cs,
                const google::protobuf::RepeatedPtrField<RepoSpec>& repositories);
    ~git_indexer();
    void begin_indexing();
protected:
    int get_next_repo_idx();
    void process_repos(int estimatedReposToProcess, threadsafe_progress_indicator *tpi);
    void walk(git_repository *curr_repo,
            std::string ref,
            std::string repopath,
            std::string name,
            Metadata metadata,
            bool walk_submodules,
            std::string submodule_prefix);
    void walk_tree(std::string pfx,
                   std::string order,
                   std::string repopath,
                   bool walk_submodules,
                   std::string submodule_prefix,
                   const indexed_tree *idx_tree,
                   git_tree *tree,
                   git_repository *curr_repo,
                   int depth);
    void index_files();
    void print_last_git_err_and_exit(int err);
    void process_trees(int thread_id);

    code_searcher *cs_;
    std::string submodule_prefix_;
    const google::protobuf::RepeatedPtrField<RepoSpec>& repositories_to_index_;
    const int repositories_to_index_length_;
    std::atomic<int> next_repo_to_process_idx_{0};
    std::mutex files_mutex_;
    std::vector<pre_indexed_file*> files_to_index_;
    std::vector<git_repository *> open_git_repos_;
    std::vector<std::thread> threads_;
    thread_queue<tree_to_walk*> trees_to_walk_;
    thread_queue<pre_indexed_file*> fq_;
};

#endif
