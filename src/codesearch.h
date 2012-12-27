/********************************************************************
 * livegrep -- codesearch.h
 * Copyright (c) 2011-2012 Nelson Elhage
 * All Rights Reserved
 ********************************************************************/
#ifndef CODESEARCH_H
#define CODESEARCH_H

#include <vector>
#include <string>
#include <map>
#include <fstream>

#ifdef USE_DENSE_HASH_SET
#include <google/dense_hash_set>
#else
#include <google/sparse_hash_set>
#endif
#include <google/sparse_hash_map>
#include <re2/re2.h>
#include <locale>

#include "smart_git.h"
#include "mutex.h"
#include "thread_pool.h"
#include "content.h"

class searcher;
class chunk_allocator;
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


bool operator==(const git_oid &lhs, const git_oid &rhs);

struct hashoid {
    size_t operator()(const git_oid &oid) const;
};

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

struct chunk;
struct chunk_file;

struct git_path {
    const string *repo_ref;
    string path;
};

struct search_file {
    vector<git_path> paths;
    git_oid oid;
    file_contents *content;
    int no;
};

struct match_context {
    search_file *file;
    vector<git_path> paths;
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
    void walk_ref(git_repository *repo, const char *ref);
    void dump_stats();
    void dump_index(const string& path);
    void load_index(const string& path);

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

        void match_internal(RE2& pat, RE2 *file_pat, const base_cb& cb, match_stats *stats);
    public:
        search_thread(code_searcher *cs);
        ~search_thread();

        /* file_pat may be NULL */
        template <class T>
        void match(RE2& pat, RE2 *file_pat, T cb, match_stats *stats) {
            match_internal(pat, file_pat, match_cb<T>(cb), stats);
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
    void walk_root(git_repository *repo, const string *ref, git_tree *tree);
    void walk_tree(git_repository *repo, const string *ref,
                   const string& pfx, git_tree *tree);
    void update_stats(const string *ref, const string& path, git_blob *blob);

    string_hash lines_;
    google::sparse_hash_map<git_oid, search_file*, hashoid> file_map_;
    struct {
        unsigned long bytes, dedup_bytes;
        unsigned long lines, dedup_lines;
        unsigned long files, dedup_files;
    } stats_;
    chunk_allocator *alloc_;
    bool finalized_;
    std::vector<string>  refs_;
    std::vector<search_file*> files_;

    friend class search_thread;
    friend class searcher;
    friend class codesearch_index;
    friend class load_allocator;
};

// dump_load.cc
chunk_allocator *make_dump_allocator(code_searcher *search, const string& path);
// chunk_allocator.cc
chunk_allocator *make_mem_allocator();

#endif /* CODESEARCH_H */
