/********************************************************************
 * livegrep -- codesearch.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_H
#define CODESEARCH_H

#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <boost/intrusive_ptr.hpp>

#ifdef USE_DENSE_HASH_SET
#include <google/dense_hash_set>
#else
#include <google/sparse_hash_set>
#endif
#include <google/sparse_hash_map>
#include <re2/re2.h>
#include <locale>

#include "mutex.h"
#include "thread_pool.h"

class searcher;
class chunk_allocator;
class file_contents;
struct match_result;

using re2::RE2;
using re2::StringPiece;

using std::string;
using std::locale;
using std::vector;
using std::map;
using std::pair;

/*
 * We special-case data() == NULL to provide an "empty" element for
 * dense_hash_set.
 *
 * StringPiece::operator== will consider a zero-length string equal to a
 * zero-length string with a NULL data().
 */
struct eqstr {
    bool operator()(const StringPiece& lhs, const StringPiece& rhs) const;
};

struct hashstr {
    locale loc;
    size_t operator()(const StringPiece &str) const;
};

struct sha1_buf {
    unsigned char hash[20];
};

bool operator==(const sha1_buf &lhs, const sha1_buf &rhs);

struct hash_sha1 {
    size_t operator()(const sha1_buf &hash) const;
};

void sha1_string(sha1_buf *out, StringPiece string);

#ifdef USE_DENSE_HASH_SET
typedef google::dense_hash_set<StringPiece, hashstr, eqstr> string_hash;
#else
typedef google::sparse_hash_set<StringPiece, hashstr, eqstr> string_hash;
#endif

enum exit_reason {
    kExitNone = 0,
    kExitTimeout,
    kExitMatchLimit,
};


struct match_stats {
    timeval re2_time;
    timeval git_time;
    timeval sort_time;
    timeval index_time;
    timeval analyze_time;
    int matches;
    exit_reason why;
};

struct index_stats {
    unsigned long bytes, dedup_bytes;
    unsigned long lines, dedup_lines;
    unsigned long files, dedup_files;
    unsigned long chunks, content_chunks;
    unsigned long content_ranges;
};

struct chunk;
struct chunk_file;

struct indexed_tree {
    string name;
    int id;
};

struct indexed_path {
    indexed_tree *tree;
    string path;
};

struct indexed_file {
    vector<indexed_path> paths;
    sha1_buf hash;
    file_contents *content;
    int no;
};

struct match_context {
    indexed_file *file;
    vector<indexed_path> paths;
    int lno;
    vector<StringPiece> context_before;
    vector<StringPiece> context_after;
};

struct match_result {
    vector<match_context> context;
    StringPiece line;
    int matchleft, matchright;
};

class code_searcher {
public:
    code_searcher();
    ~code_searcher();
    void dump_stats();
    void dump_index(const string& path);
    void load_index(const string& path);

    void index_file(const string& tree,
                    const string& path,
                    StringPiece contents);
    void finalize();

    void set_alloc(chunk_allocator *alloc);

    class search_thread {
    protected:
        struct base_cb {
            virtual void operator()(const struct match_result *m) const = 0;
        };
        template <class T>
        struct match_cb : public base_cb {
            match_cb(T cb) : cb_(cb) {}
            virtual void operator()(const struct match_result *m) const {
                cb_(m);
            }
        private:
            T cb_;
        };

        void match_internal(RE2& pat, RE2 *file_pat, RE2 *tree_pat,
                            const base_cb& cb, match_stats *stats);
    public:
        search_thread(code_searcher *cs);
        ~search_thread();

        /* file_pat may be NULL */
        template <class T>
        void match(RE2& pat, RE2 *file_pat, RE2 *tree_pat, T cb, match_stats *stats) {
            match_internal(pat, file_pat, tree_pat, match_cb<T>(cb), stats);
        }
    protected:
        const code_searcher *cs_;
        thread_pool<pair<searcher*, chunk*>,
                    bool(*)(const pair<searcher*, chunk*>&)> pool_;

        static bool search_one(const pair<searcher*, chunk*>& pair);
    private:
        search_thread(const search_thread&);
        void operator=(const search_thread&);
    };

protected:
    string_hash lines_;
    index_stats stats_;
    chunk_allocator *alloc_;
    bool finalized_;
    vector<indexed_tree*> trees_;
    map<string, indexed_tree*> tree_map_;
    vector<indexed_file*> files_;
    google::sparse_hash_map<sha1_buf, indexed_file*, hash_sha1> file_map_;

    friend class search_thread;
    friend class searcher;
    friend class codesearch_index;
    friend class load_allocator;
};

// dump_load.cc
chunk_allocator *make_dump_allocator(code_searcher *search, const string& path);
// chunk_allocator.cc
chunk_allocator *make_mem_allocator();

void default_re2_options(RE2::Options&);

#endif /* CODESEARCH_H */
