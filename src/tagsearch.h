/********************************************************************
 * livegrep -- tagsearch.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef TAGSEARCH_H
#define TAGSEARCH_H

#include "codesearch.h"

#include <map>
#include <string>

class RE2;
class chunk_allocator;

class tag_searcher {
public:
    void load_index(const string& path);

    void cache_indexed_files(code_searcher *cs);

    std::string create_partial_regex(RE2 *re) const;

    std::string create_tag_line_regex(
        const std::string& name,
        const std::string& file,
        const std::string& lno,
        const std::string& tags) const;

    bool transform(query *q, match_result *m) const;

    code_searcher* cs() { return &cs_; }

protected:
    code_searcher cs_;
    chunk_allocator *file_alloc_;
    typedef std::pair<std::string, std::string> repo_path_pair;
    std::map<repo_path_pair, indexed_file*> path_to_file_map_;
};

#endif /* TAGSEARCH_H */
