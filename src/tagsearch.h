/********************************************************************
 * livegrep -- tagsearch.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef TAGSEARCH_H
#define TAGSEARCH_H

#include "src/codesearch.h"

#include <map>
#include <string>

class chunk_allocator;

class tag_searcher {
public:
    void cache_indexed_files(code_searcher *cs);

    bool transform(query *q, match_result *m) const;

    static std::string create_tag_line_regex_from_query(query *q);

protected:
    chunk_allocator *file_alloc_;
    std::map<std::string, indexed_file*> path_to_file_map_;
};

#endif /* TAGSEARCH_H */
