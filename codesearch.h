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
#include <re2/re2.h>
#include <locale>

#include "smart_git.h"
#include "mutex.h"
#include "thread_pool.h"

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


#ifdef USE_DENSE_HASH_SET
typedef google::dense_hash_set<StringPiece, hashstr, eqstr> string_hash;
#else
typedef google::sparse_hash_set<StringPiece, hashstr, eqstr> string_hash;
#endif

struct match_stats {
    timeval re2_time;
    timeval git_time;
    timeval sort_time;
    timeval index_time;
    timeval analyze_time;
};

enum exit_reason {
    kExitNone = 0,
    kExitTimeout,
    kExitMatchLimit,
};

struct chunk;
struct chunk_file;

struct search_file {
    string path;
    const char *ref;
    git_oid oid;
    vector<StringPiece> content;
    int no;
};

class code_searcher {
public:
    code_searcher();
    ~code_searcher();
    void walk_ref(git_repository *repo, const char *ref);
    void dump_stats();
    void dump_index(const string& path);
    void load_index(const string& path);

    void set_output_json(bool j) { output_json_ = j; }
    void finalize();

    class search_thread {
    public:
        search_thread(code_searcher *cs);
        ~search_thread();
        int match(RE2& pat, match_stats *stats, exit_reason *why);
    protected:
        void print_match(const match_result *m);
        void print_match_json(const match_result *m);

        const code_searcher *cs_;
        thread_pool<pair<searcher*, chunk*>,
                    bool(*)(const pair<searcher*, chunk*>&)> pool_;

        static bool search_one(const pair<searcher*, chunk*>& pair);
    };
    friend class search_thread;
protected:
    void walk_tree(git_repository *repo, const char *ref, const string& pfx, git_tree *tree);
    void update_stats(const char *ref, const string& path, git_blob *blob);

    void dump_file(std::ostream& stream, search_file *sf);
    void dump_file_contents(std::ostream& stream, map<chunk*, int>&, search_file *sf);
    void dump_chunk(std::ostream& stream, chunk *);

    search_file *load_file(std::istream& stream);
    void load_file_contents(std::istream& stream, vector<chunk*>&, search_file *sf);
    void load_chunk(std::istream& stream, chunk *);

    string_hash lines_;
    struct {
        unsigned long bytes, dedup_bytes;
        unsigned long lines, dedup_lines;
    } stats_;
    chunk_allocator *alloc_;
    bool output_json_;
    bool finalized_;
    std::vector<const char*>  refs_;
    std::vector<search_file*> files_;

    friend class searcher;
};


#endif /* CODESEARCH_H */
