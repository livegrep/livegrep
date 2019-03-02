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
#include "src/proto/config.pb.h"

class code_searcher;
struct indexed_tree;
namespace boost { namespace filesystem { class path; } }


class fs_indexer {
public:
    fs_indexer(code_searcher *cs,
               const string& repopath,
               const string& name,
               const Metadata &metadata);
    ~fs_indexer();
    void walk(const boost::filesystem::path& path);
    void walk_contents_file(const boost::filesystem::path& contents_file_path);
protected:
    code_searcher *cs_;
    std::string repopath_;
    std::string name_;
    const indexed_tree *tree_;

    void read_file(const boost::filesystem::path& path);
};

#endif
