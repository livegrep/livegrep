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
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <memory>
#include <boost/intrusive_ptr.hpp>

#include "absl/hash/hash.h"
#include "absl/container/flat_hash_set.h"
#include "re2/re2.h"

#include "src/lib/thread_queue.h"
#include "src/proto/config.pb.h"

class searcher;
class filename_searcher;
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
using std::atomic_int;

struct hashstr {
    size_t operator()(const StringPiece &str) const;
};

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

    match_stats() : re2_time((struct timeval){0}),
        git_time((struct timeval){0}),
        sort_time((struct timeval){0}),
        index_time((struct timeval){0}),
        analyze_time((struct timeval){0}),
        matches(0),
        why(kExitNone) {}
};

struct chunk;
struct chunk_file;

struct indexed_tree {
    string name;
    Metadata metadata;
    string version;
};

struct indexed_file {
    const indexed_tree *tree;
    string path;
    file_contents *content;
    int no;
};

struct index_info {
    std::string name;
    vector<indexed_tree> trees;
};

struct match_result {
    indexed_file *file;
    int lno;
    vector<StringPiece> context_before;
    vector<StringPiece> context_after;
    StringPiece line;
    int matchleft, matchright;
};

struct file_result {
    indexed_file *file;
    int matchleft, matchright;
};

// A query specification passed to match(). line_pat is required to be
// non-NULL; file_pat, tree_pat and tag_pat may be NULL to specify "no
// constraint"
struct query {
    std::string trace_id;
    int32_t max_matches;

    std::shared_ptr<RE2> line_pat;
    std::shared_ptr<RE2> file_pat;
    std::shared_ptr<RE2> tree_pat;
    std::shared_ptr<RE2> tags_pat;
    struct {
        std::shared_ptr<RE2> file_pat;
        std::shared_ptr<RE2> tree_pat;
        std::shared_ptr<RE2> tags_pat;
    } negate;

    bool filename_only;
    int context_lines;
};

class code_searcher {
public:
    code_searcher();
    ~code_searcher();
    void dump_index(const string& path);
    void load_index(const string& path);

    const indexed_tree *open_tree(const string &name, const Metadata &meta, const string& version);
    const indexed_tree *open_tree(const string &name, const string& version);

    void index_file(const indexed_tree *tree,
                    const string& path,
                    StringPiece contents);
    void finalize();

    void set_alloc(std::unique_ptr<chunk_allocator> alloc);
    chunk_allocator *alloc() { return alloc_.get(); }

    vector<indexed_tree> trees() const;
    string name() const {
        return name_;
    };
    void set_name(const string& name) {
        name_ = name;
    }

    vector<std::unique_ptr<indexed_file>>::const_iterator begin_files() {
        return files_.begin();
    }
    vector<std::unique_ptr<indexed_file>>::const_iterator end_files() {
        return files_.end();
    }

    int64_t index_timestamp() {
        return index_timestamp_;
    }

    class search_thread {
    public:
        search_thread(code_searcher *cs);
        ~search_thread();

        // function that will be called to record a match
        typedef std::function<void (const struct match_result*)> callback_func;
        // function that will be called to record a filename match
        typedef std::function<void (const struct file_result*)> file_callback_func;
        // function that will be called to transform a match
        typedef std::function<bool (struct match_result*)> transform_func;

        /* file_pat may be NULL */
        void match(const query& q,
                   const callback_func& cb,
                   const file_callback_func& fcb,
                   match_stats *stats)
        {
            match(q, cb, fcb, transform_func(), stats);
        }
        void match(const query& q,
                   const callback_func& cb,
                   const file_callback_func& fcb,
                   const transform_func& func,
                   match_stats *stats);
    protected:
        struct job {
            std::string trace_id;
            atomic_int pending;
            searcher *search;
            filename_searcher *file_search;
            thread_queue<chunk*> chunks;
        };

        const code_searcher *cs_;
        vector<std::thread> threads_;
        thread_queue<job*> queue_;
        thread_queue<job*> file_queue_;

        static void search_one(search_thread *);
        static void search_file_one(search_thread *);
    private:
        search_thread(const search_thread&);
        void operator=(const search_thread&);
    };

protected:
    string name_;

    // Transient structure used during index construction to dedup lines.
    // Looking up a StringPiece here will find an equivalent StringPiece
    // already stored in some existing chunk's data, if such a StringPiece is
    // present.
    absl::flat_hash_set<StringPiece, hashstr> lines_;

    std::unique_ptr<chunk_allocator> alloc_;

    // Indicates that everything all is ready for searching--we are done creating
    // index or initializing it from a file.
    bool finalized_;

    // Timestamp representing the end of index construction.
    int64_t index_timestamp_;

    // Structures for fast filename search; somewhat similar to a single chunk.
    // Built from files_ at finalization, not serialized or anything like that.
    vector<unsigned char> filename_data_;
    vector<uint32_t> filename_suffixes_;
    // pairs (i, file), where file->path starts at filename_data_[i]
    vector<pair<int, indexed_file*>> filename_positions_;

    vector<std::unique_ptr<indexed_tree>> trees_;
    vector<std::unique_ptr<indexed_file>> files_;

private:
    void index_filenames();

    friend class search_thread;
    friend class searcher;
    friend class filename_searcher;
    friend class codesearch_index;
    friend class load_allocator;
    friend class tag_searcher;
};

// dump_load.cc
std::unique_ptr<chunk_allocator> make_dump_allocator(code_searcher *search, const string& path);
// chunk_allocator.cc
std::unique_ptr<chunk_allocator> make_mem_allocator();

void default_re2_options(RE2::Options&);

#endif /* CODESEARCH_H */
