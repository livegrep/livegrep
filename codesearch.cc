#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <locale>
#include <list>
#include <iostream>
#include <string>
#include <atomic>

#include <re2/re2.h>
#include <re2/filtered_re2.h>

#include <json/json.h>

#include <gflags/gflags.h>

#include "timer.h"
#include "thread_queue.h"
#include "thread_pool.h"
#include "codesearch.h"
#include "chunk.h"
#include "chunk_allocator.h"

#include "utf8.h"

using re2::RE2;
using re2::StringPiece;
using namespace std;

const int    kMaxMatches   = 50;
const int    kContextLines = 3;

#ifdef PROFILE_CODESEARCH
#define log_profile(format, ...) fprintf(stderr, format, __VA_ARGS__)
#else
#define log_profile(...)
#endif

DEFINE_bool(index, true, "Create a suffix-array index to speed searches.");
DECLARE_int32(threads);

struct search_file {
    string path;
    const char *ref;
    git_oid oid;
};

struct match_result {
    search_file *file;
    int lno;
    vector<string> context_before;
    vector<string> context_after;
    StringPiece line;
    int matchleft, matchright;
};

bool eqstr::operator()(const StringPiece& lhs, const StringPiece& rhs) const {
    if (lhs.data() == NULL && rhs.data() == NULL)
        return true;
    if (lhs.data() == NULL || rhs.data() == NULL)
        return false;
    return lhs == rhs;
}

size_t hashstr::operator()(const StringPiece& str) const {
    const std::collate<char>& coll = std::use_facet<std::collate<char> >(loc);
    return coll.hash(str.data(), str.data() + str.size());
}

const StringPiece empty_string(NULL, 0);

class code_searcher;

class searcher {
public:
    searcher(code_searcher *cc, thread_queue<match_result*>& queue, RE2& pat) :
        cc_(cc), pat_(pat), queue_(queue),
        matches_(0), re2_time_(false), git_time_(false)
    {
        int id;
        re2::FilteredRE2 fre2;
        assert(!fre2.Add(pat.pattern(), pat.options(), &id));
        fre2.Compile(&filter_, false);
    }

    ~searcher() {
        log_profile("re2 time: %d.%06ds\n",
                    int(re2_time_.elapsed().tv_sec),
                    int(re2_time_.elapsed().tv_usec));
        log_profile("git time: %d.%06ds\n",
                    int(git_time_.elapsed().tv_sec),
                    int(git_time_.elapsed().tv_usec));
    }

    class thread_state {
    public:
        thread_state(const searcher& search) {
            git_repository_open(&repo_,
                                git_repository_path(search.cc_->repo_,
                                                    GIT_REPO_PATH));
            assert(repo_);
        }
        ~thread_state() {
            git_repository_free(repo_);
        }
    protected:
        thread_state(const thread_state&);
        thread_state operator=(const thread_state&);
        git_repository *repo_;
        friend class searcher;
    };

    bool operator()(const thread_state& ts, const chunk *chunk);

    void get_stats(match_stats *stats) {
        stats->re2_time = re2_time_.elapsed();
        stats->git_time = git_time_.elapsed();
    }

protected:
    void full_search(const thread_state& ts, const chunk *chunk);
    void filtered_search(const thread_state& ts, const chunk *chunk);
    void find_match (const chunk *chunk,
                     const StringPiece& match,
                     const StringPiece& line,
                     const thread_state& ts) {
        run_timer run(git_time_);
        timer tm;
        int off = line.data() - chunk->data;
        int searched = 0;
        bool found = false;
        for(vector<chunk_file>::const_iterator it = chunk->files.begin();
            it != chunk->files.end(); it++) {
            if (off >= it->left && off <= it->right) {
                searched++;
                if (matches_.load() >= kMaxMatches)
                    break;
                match_result *m = try_match(line, match, it->file, ts.repo_);
                if (m) {
                    found = true;
                    queue_.push(m);
                    ++matches_;
                }
            }
        }
        assert(found || matches_.load() >= kMaxMatches);
        tm.pause();
        log_profile("Searched %d files in %d.%06ds\n",
                    searched,
                    int(tm.elapsed().tv_sec),
                    int(tm.elapsed().tv_usec));
    }

    match_result *try_match(const StringPiece&, const StringPiece&,
                            search_file *, git_repository *);

    static StringPiece find_line(const StringPiece& chunk, const StringPiece& match) {
        const char *start, *end;
        assert(match.data() >= chunk.data());
        assert(match.data() <= chunk.data() + chunk.size());
        assert(match.size() <= (chunk.size() - (match.data() - chunk.data())));
        start = static_cast<const char*>
            (memrchr(chunk.data(), '\n', match.data() - chunk.data()));
        if (start == NULL)
            start = chunk.data();
        else
            start++;
        end = static_cast<const char*>
            (memchr(match.data() + match.size(), '\n',
                    chunk.size() - (match.data() - chunk.data()) - match.size()));
        if (end == NULL)
            end = chunk.data() + chunk.size();
        return StringPiece(start, end - start);
    }

    code_searcher *cc_;
    RE2& pat_;
    thread_queue<match_result*>& queue_;
    atomic_int matches_;
    vector<string> filter_;
    timer re2_time_;
    timer git_time_;
};

code_searcher::code_searcher(git_repository *repo)
    : repo_(repo), stats_(), output_json_(false), finalized_(false)
{
#ifdef USE_DENSE_HASH_SET
    lines_.set_empty_key(empty_string);
#endif
    alloc_ = new chunk_allocator();
}

code_searcher::~code_searcher() {
    delete alloc_;
}

void code_searcher::walk_ref(const char *ref) {
    assert(!finalized_);
    smart_object<git_commit> commit;
    smart_object<git_tree> tree;
    resolve_ref(commit, ref);
    git_commit_tree(tree, commit);

    walk_tree(ref, "", tree);
}

void code_searcher::dump_stats() {
    log_profile("chunk_files: %d\n", chunk::chunk_files);
    printf("Bytes: %ld (dedup: %ld)\n", stats_.bytes, stats_.dedup_bytes);
    printf("Lines: %ld (dedup: %ld)\n", stats_.lines, stats_.dedup_lines);
}

int code_searcher::match(RE2& pat, match_stats *stats) {
    list<chunk*>::iterator it;
    match_result *m;
    int matches = 0;
    int threads = FLAGS_threads;

    assert(finalized_);

    thread_queue<match_result*> results;
    searcher search(this, results, pat);
    thread_pool<chunk*, searcher, searcher::thread_state> pool(threads, search);

    for (it = alloc_->begin(); it != alloc_->end(); it++) {
        pool.queue(*it);
    }
    for (int i = 0; i < threads; i++)
        pool.queue(NULL);

    while (threads) {
        m = results.pop();
        if (!m) {
            threads--;
            continue;
        }
        matches++;
        print_match(m);
        delete m;
    }

    search.get_stats(stats);
    return matches;
}

void code_searcher::finalize() {
    assert(!finalized_);
    finalized_ = true;
    alloc_->finalize();
}

void code_searcher::print_match(const match_result *m) {
    if (output_json_)
        print_match_json(m);
    else
        printf("%s:%s:%d:%d-%d: %.*s\n",
               m->file->ref,
               m->file->path.c_str(),
               m->lno,
               m->matchleft, m->matchright,
               m->line.size(), m->line.data());
}

static json_object *to_json(vector<string> vec) {
    json_object *out = json_object_new_array();
    for (vector<string>::iterator it = vec.begin(); it != vec.end(); it++)
        json_object_array_add(out, json_object_new_string(it->c_str()));
    return out;
}

void code_searcher::print_match_json(const match_result *m) {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "ref",  json_object_new_string(m->file->ref));
    json_object_object_add(obj, "file", json_object_new_string(m->file->path.c_str()));
    json_object_object_add(obj, "lno",  json_object_new_int(m->lno));
    json_object *bounds = json_object_new_array();
    json_object_array_add(bounds, json_object_new_int(m->matchleft));
    json_object_array_add(bounds, json_object_new_int(m->matchright));
    json_object_object_add(obj, "bounds", bounds);
    json_object_object_add(obj, "line",
                           json_object_new_string_len(m->line.data(),
                                                      m->line.size()));
    json_object_object_add(obj, "context_before",
                           to_json(m->context_before));
    json_object_object_add(obj, "context_after",
                           to_json(m->context_after));
    printf("%s\n", json_object_to_json_string(obj));
    json_object_put(obj);
}

void code_searcher::walk_tree(const char *ref, const string& pfx, git_tree *tree) {
    string path;
    int entries = git_tree_entrycount(tree);
    int i;
    for (i = 0; i < entries; i++) {
        const git_tree_entry *ent = git_tree_entry_byindex(tree, i);
        path = pfx + git_tree_entry_name(ent);
        smart_object<git_object> obj;
        git_tree_entry_2object(obj, repo_, ent);
        if (git_tree_entry_type(ent) == GIT_OBJ_TREE) {
            walk_tree(ref, path + "/", obj);
        } else if (git_tree_entry_type(ent) == GIT_OBJ_BLOB) {
            update_stats(ref, path, obj);
        }
    }
}

void code_searcher::update_stats(const char *ref, const string& path, git_blob *blob) {
    size_t len = git_blob_rawsize(blob);
    const char *p = static_cast<const char*>(git_blob_rawcontent(blob));
    const char *end = p + len;
    const char *f;
    search_file *sf = new search_file;
    sf->path = path;
    sf->ref = ref;
    git_oid_cpy(&sf->oid, git_object_id(reinterpret_cast<git_object*>(blob)));
    chunk *c;
    StringPiece line;

    if (memchr(p, 0, len) != NULL)
        return;

    while ((f = static_cast<const char*>(memchr(p, '\n', end - p))) != 0) {
        string_hash::iterator it = lines_.find(StringPiece(p, f - p));
        if (it == lines_.end()) {
            stats_.dedup_bytes += (f - p) + 1;
            stats_.dedup_lines ++;

            // Include the trailing '\n' in the chunk buffer
            char *alloc = alloc_->alloc(f - p + 1);
            memcpy(alloc, p, f - p + 1);
            line = StringPiece(alloc, f - p);
            lines_.insert(line);
            c = alloc_->current_chunk();
        } else {
            line = *it;
            c = chunk::from_str(line.data());
        }
        c->add_chunk_file(sf, line);
        p = f + 1;
        stats_.lines++;
    }

    for (list<chunk*>::iterator it = alloc_->begin();
         it != alloc_->end(); it++)
        (*it)->finish_file();

    stats_.bytes += len;
}

void code_searcher::resolve_ref(smart_object<git_commit>& out, const char *refname) {
    git_reference *ref;
    const git_oid *oid;
    git_oid tmp;
    smart_object<git_object> obj;
    if (git_oid_fromstr(&tmp, refname) == GIT_SUCCESS) {
        git_object_lookup(obj, repo_, &tmp, GIT_OBJ_ANY);
    } else {
        git_reference_lookup(&ref, repo_, refname);
        git_reference_resolve(&ref, ref);
        oid = git_reference_oid(ref);
        git_object_lookup(obj, repo_, oid, GIT_OBJ_ANY);
    }
    if (git_object_type(obj) == GIT_OBJ_TAG) {
        git_tag_target(out, obj);
    } else {
        out = obj.release();
    }
}

bool searcher::operator()(const thread_state& ts, const chunk *chunk)
{
    if (chunk == NULL) {
        queue_.push(NULL);
        return true;
    }

    if (FLAGS_index && filter_.size() > 0 && filter_.size() < 4)
        filtered_search(ts, chunk);
    else
        full_search(ts, chunk);

    if (matches_.load() >= kMaxMatches) {
        queue_.push(NULL);
        return true;
    }
    return false;
}

void searcher::filtered_search(const thread_state& ts, const chunk *chunk)
{
    log_profile("Attempting filtered search with %d filters\n", int(filter_.size()));
    chunk::lt_suffix lt(chunk);

    for (vector<string>::iterator it = filter_.begin();
         it != filter_.end(); it++) {
        pair<uint32_t*,uint32_t*> range = equal_range
            (chunk->suffixes, chunk->suffixes + chunk->size,
             *it, lt);
        uint32_t *l = range.first, *r = range.second;
        log_profile("%s: found %d potential matches.\n",
                    it->c_str(), int(r - l));
        StringPiece search(chunk->data, chunk->size);
        for (; l < r && matches_.load() < kMaxMatches; l++) {
            StringPiece line = find_line(search, StringPiece(chunk->data + *l, 0));
            StringPiece match;
            if (!utf8::is_valid(line.data(), line.data() + line.size()))
                continue;
            if (!pat_.Match(line, 0, line.size(), RE2::UNANCHORED, &match, 1))
                continue;
            find_match(chunk, match, line, ts);
        }
    }
}

void searcher::full_search(const thread_state& ts, const chunk *chunk)
{
    StringPiece str(chunk->data, chunk->size);
    StringPiece match;
    int pos = 0, new_pos;
    while (pos < str.size() && matches_.load() < kMaxMatches) {
        {
            run_timer run(re2_time_);
            if (!pat_.Match(str, pos, str.size() - 1, RE2::UNANCHORED, &match, 1))
                break;
        }
        assert(memchr(match.data(), '\n', match.size()) == NULL);
        StringPiece line = find_line(str, match);
        if (utf8::is_valid(line.data(), line.data() + line.size()))
            find_match(chunk, match, line, ts);
        new_pos = line.size() + line.data() - str.data() + 1;
        assert(new_pos > pos);
        pos = new_pos;
    }
}


match_result *searcher::try_match(const StringPiece& line,
                                  const StringPiece& match,
                                  search_file *sf,
                                  git_repository *repo) {
    smart_object<git_blob> blob;
    git_blob_lookup(blob, repo, &sf->oid);
    StringPiece search(static_cast<const char*>(git_blob_rawcontent(blob)),
                       git_blob_rawsize(blob));
    StringPiece matchline;
    RE2 pat("^" + RE2::QuoteMeta(line) + "$", pat_.options());
    assert(pat.ok());
    if (!pat.Match(search, 0, search.size(), RE2::UNANCHORED, &matchline, 1))
        return 0;
    match_result *m = new match_result;
    m->file = sf;
    m->lno  = 1 + count(search.data(), matchline.data(), '\n');
    m->line = line;
    m->matchleft = int(match.data() - line.data());
    m->matchright = m->matchleft + match.size();
    StringPiece l = matchline;
    int i;
    for (i = 0; i < kContextLines && l.data() > search.data(); i++) {
        l = find_line(search, StringPiece(l.data() - 1, 0));
        m->context_before.push_back(l.as_string());
    }
    l = matchline;
    for (i = 0; i < kContextLines &&
                    (l.data() + l.size()) < (search.data() + search.size()); i++) {
        l = find_line(search, StringPiece(l.data() + l.size() + 1, 0));
        m->context_after.push_back(l.as_string());
    }
    return m;
}
