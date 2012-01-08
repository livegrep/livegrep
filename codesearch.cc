#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <locale>
#include <list>
#include <iostream>
#include <string>
#include <fstream>

#include <re2/re2.h>

#include <json/json.h>

#include <gflags/gflags.h>

#include "timer.h"
#include "thread_queue.h"
#include "thread_pool.h"
#include "codesearch.h"
#include "chunk.h"
#include "chunk_allocator.h"
#include "radix_sort.h"
#include "atomic.h"
#include "indexer.h"

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
DEFINE_bool(search, true, "Actually do the search.");
DEFINE_bool(quiet, false, "Do the search, but don't print results.");
DEFINE_int32(timeout, 1, "The number of seconds a single search may run for.");
DECLARE_int32(threads);

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
    searcher(const code_searcher *cc, thread_queue<match_result*>& queue, RE2& pat) :
        cc_(cc), pat_(pat), queue_(queue),
        matches_(0), re2_time_(false), git_time_(false),
        index_time_(false), sort_time_(false), analyze_time_(false),
        exit_reason_(kExitNone)
    {
        {
            run_timer run(analyze_time_);
            index_ = indexRE(pat);
            log_profile("Index: %s\n", index_->ToString().c_str());
        }

        if (FLAGS_timeout <= 0) {
            limit_.tv_sec = numeric_limits<time_t>::max();
        } else {
            gettimeofday(&limit_, NULL);
            limit_.tv_sec += FLAGS_timeout;
        }
    }

    ~searcher() {
        log_profile("re2 time: %d.%06ds\n",
                    int(re2_time_.elapsed().tv_sec),
                    int(re2_time_.elapsed().tv_usec));
        log_profile("git time: %d.%06ds\n",
                    int(git_time_.elapsed().tv_sec),
                    int(git_time_.elapsed().tv_usec));
        log_profile("index time: %d.%06ds\n",
                    int(index_time_.elapsed().tv_sec),
                    int(index_time_.elapsed().tv_usec));
        log_profile("sort time: %d.%06ds\n",
                    int(sort_time_.elapsed().tv_sec),
                    int(sort_time_.elapsed().tv_usec));
        log_profile("analyze time: %d.%06ds\n",
                    int(analyze_time_.elapsed().tv_sec),
                    int(analyze_time_.elapsed().tv_usec));
    }

    void operator()(const chunk *chunk);

    void get_stats(match_stats *stats) {
        stats->re2_time = re2_time_.elapsed();
        stats->git_time = git_time_.elapsed();
        stats->index_time = index_time_.elapsed();
        stats->sort_time  = sort_time_.elapsed();
        stats->analyze_time  = analyze_time_.elapsed();
    }

    exit_reason why() {
        return exit_reason_;
    }

protected:
    void full_search(const chunk *chunk);
    void full_search(const chunk *chunk, size_t minpos, size_t maxpos);

    void filtered_search(const chunk *chunk);
    void search_lines(uint32_t *left, int count, const chunk *chunk);

    void find_match (const chunk *chunk,
                     const StringPiece& match,
                     const StringPiece& line) {
        run_timer run(git_time_);
        timer tm;
        int off = (unsigned char*)line.data() - chunk->data;
        int searched = 0;
        bool found = false;
        for(vector<chunk_file>::const_iterator it = chunk->files.begin();
            it != chunk->files.end(); it++) {
            if (off >= it->left && off <= it->right) {
                searched++;
                if (exit_early())
                    break;
                match_result *m = try_match(line, match, it->file);
                if (m) {
                    found = true;
                    queue_.push(m);
                    ++matches_;
                }
            }
        }
        assert(found || exit_reason_);
        tm.pause();
        log_profile("Searched %d files in %d.%06ds\n",
                    searched,
                    int(tm.elapsed().tv_sec),
                    int(tm.elapsed().tv_usec));
    }

    match_result *try_match(const StringPiece&, const StringPiece&,
                            search_file *);

    static int line_start(const chunk *chunk, int pos) {
        const unsigned char *start = static_cast<const unsigned char*>
            (memrchr(chunk->data, '\n', pos));
        if (start == NULL)
            return 0;
        return start - chunk->data;
    }

    static int line_end(const chunk *chunk, int pos) {
        const unsigned char *end = static_cast<const unsigned char*>
            (memchr(chunk->data + pos, '\n', chunk->size - pos));
        if (end == NULL)
            return chunk->size;
        return end - chunk->data;
    }

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

    bool exit_early() {
        if (exit_reason_)
            return true;

        if (matches_.load() >= kMaxMatches) {
            exit_reason_ = kExitMatchLimit;
            return true;
        }
#ifdef CODESEARCH_SLOWGTOD
        static int counter = 1000;
        if (--counter)
            return false;
        counter = 1000;
#endif
        timeval now;
        gettimeofday(&now, NULL);
        if (now.tv_sec > limit_.tv_sec ||
            (now.tv_sec == limit_.tv_sec && now.tv_usec > limit_.tv_usec)) {
            exit_reason_ = kExitTimeout;
            return true;
        }
        return false;
    }

    const code_searcher *cc_;
    RE2& pat_;
    thread_queue<match_result*>& queue_;
    atomic_int matches_;
    intrusive_ptr<IndexKey> index_;
    timer re2_time_;
    timer git_time_;
    timer index_time_;
    timer sort_time_;
    timer analyze_time_;
    timeval limit_;
    exit_reason exit_reason_;

    friend class code_searcher::search_thread;
};

code_searcher::code_searcher()
    : stats_(), output_json_(false),
      finalized_(false)
{
#ifdef USE_DENSE_HASH_SET
    lines_.set_empty_key(empty_string);
#endif
    alloc_ = new chunk_allocator();
}

code_searcher::~code_searcher() {
    delete alloc_;
}

namespace {
    void resolve_ref(git_repository *repo,
                     smart_object<git_commit>& out,
                     const char *refname) {
        git_reference *ref;
        const git_oid *oid;
        git_oid tmp;
        smart_object<git_object> obj;
        if (git_oid_fromstr(&tmp, refname) == GIT_SUCCESS) {
            git_object_lookup(obj, repo, &tmp, GIT_OBJ_ANY);
        } else {
            git_reference_lookup(&ref, repo, refname);
            git_reference_resolve(&ref, ref);
            oid = git_reference_oid(ref);
            git_object_lookup(obj, repo, oid, GIT_OBJ_ANY);
        }
        if (git_object_type(obj) == GIT_OBJ_TAG) {
            git_tag_target(out, obj);
        } else {
            out = obj.release();
        }
    }
};

void code_searcher::walk_ref(git_repository *repo, const char *ref) {
    assert(!finalized_);
    smart_object<git_commit> commit;
    smart_object<git_tree> tree;
    resolve_ref(repo, commit, ref);
    git_commit_tree(tree, commit);

    refs_.push_back(ref);

    walk_tree(repo, ref, "", tree);
}

void code_searcher::dump_stats() {
    log_profile("chunk_files: %d\n", chunk::chunk_files);
    printf("Bytes: %ld (dedup: %ld)\n", stats_.bytes, stats_.dedup_bytes);
    printf("Lines: %ld (dedup: %ld)\n", stats_.lines, stats_.dedup_lines);
}

void code_searcher::finalize() {
    assert(!finalized_);
    finalized_ = true;
    alloc_->finalize();
}

void code_searcher::walk_tree(git_repository *repo,
                              const char *ref,
                              const string& pfx,
                              git_tree *tree) {
    string path;
    int entries = git_tree_entrycount(tree);
    int i;
    for (i = 0; i < entries; i++) {
        const git_tree_entry *ent = git_tree_entry_byindex(tree, i);
        path = pfx + git_tree_entry_name(ent);
        smart_object<git_object> obj;
        git_tree_entry_2object(obj, repo, ent);
        if (git_tree_entry_type(ent) == GIT_OBJ_TREE) {
            walk_tree(repo, ref, path + "/", obj);
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
    chunk *c;
    StringPiece line;

    if (memchr(p, 0, len) != NULL)
        return;

    search_file *sf = new search_file;
    sf->path = path;
    sf->ref = ref;
    git_oid_cpy(&sf->oid, git_object_id(reinterpret_cast<git_object*>(blob)));
    sf->no  = files_.size();
    files_.push_back(sf);

    while ((f = static_cast<const char*>(memchr(p, '\n', end - p))) != 0) {
        string_hash::iterator it = lines_.find(StringPiece(p, f - p));
        if (it == lines_.end()) {
            stats_.dedup_bytes += (f - p) + 1;
            stats_.dedup_lines ++;

            // Include the trailing '\n' in the chunk buffer
            unsigned char *alloc = alloc_->alloc(f - p + 1);
            memcpy(alloc, p, f - p + 1);
            line = StringPiece((char*)alloc, f - p);
            lines_.insert(line);
            c = alloc_->current_chunk();
        } else {
            line = *it;
            c = chunk::from_str(line.data());
        }
        c->add_chunk_file(sf, line);
        if (sf->content.size() &&
            sf->content.back().data() +
            sf->content.back().size() == line.data()) {
            StringPiece &back = sf->content.back();
            assert(back.data()[back.size()] == '\n');
            back = StringPiece(back.data(),
                               (line.data() - back.data() + line.size()));
        } else {
            sf->content.push_back(StringPiece(line.data(), line.size()));
        }
        p = f + 1;
        stats_.lines++;
    }

    for (list<chunk*>::iterator it = alloc_->begin();
         it != alloc_->end(); it++)
        (*it)->finish_file();

    stats_.bytes += len;
}

void searcher::operator()(const chunk *chunk)
{
    if (exit_reason_)
        return;

    if (FLAGS_index && index_ && !index_->empty())
        filtered_search(chunk);
    else
        full_search(chunk);
}

struct walk_state {
    uint32_t *left, *right;
    intrusive_ptr<IndexKey> key;
    int depth;
};

struct lt_index {
    const chunk *chunk_;
    int idx_;

    bool operator()(uint32_t lhs, unsigned char rhs) {
        return cmp(lhs, rhs) < 0;
    }

    bool operator()(unsigned char lhs, uint32_t rhs) {
        return cmp(rhs, lhs) > 0;
    }

    int cmp(uint32_t lhs, unsigned char rhs) {
        unsigned char lc = chunk_->data[lhs + idx_];
        if (lc == '\n')
            return -1;
        return (int)lc - (int)rhs;
    }
};


void searcher::filtered_search(const chunk *chunk)
{
    uint32_t *indexes = new uint32_t[kChunkSpace];
    int count = 0;
    {
        run_timer run(index_time_);
        vector<walk_state> stack;
        stack.push_back((walk_state){
                chunk->suffixes, chunk->suffixes + chunk->size, index_, 0});

        while (!stack.empty()) {
            walk_state st = stack.back();
            stack.pop_back();
            if (!st.key || (st.right - st.left) <= 100) {
                memcpy(indexes + count, st.left,
                       (st.right - st.left) * sizeof(uint32_t));
                count += (st.right - st.left);
                continue;
            }
            lt_index lt = {chunk, st.depth};
            for (IndexKey::iterator it = st.key->begin();
                 it != st.key->end(); ++it) {
                uint32_t *l, *r;
                l = lower_bound(st.left, st.right, it->first.first, lt);
                uint32_t *right = lower_bound(l, st.right,
                                              (unsigned char)(it->first.second + 1),
                                              lt);
                if (l == right)
                    continue;

                if (st.depth)
                    assert(chunk->data[*l + st.depth - 1] ==
                           chunk->data[*(right - 1) + st.depth - 1]);

                assert(l == st.left ||
                       chunk->data[*(l-1) + st.depth] == '\n' ||
                       chunk->data[*(l-1) + st.depth] < it->first.first);
                assert(chunk->data[*l + st.depth] >= it->first.first);
                assert(right == st.right ||
                       chunk->data[*right + st.depth] > it->first.second);

                for (unsigned char ch = it->first.first; ch <= it->first.second;
                     ch++, l = r) {
                    r = lower_bound(l, right, (unsigned char)(ch + 1), lt);

                    if (r != l) {
                        stack.push_back((walk_state){l, r, it->second, st.depth + 1});
                    }
                }
            }
        }

    }

    search_lines(indexes, count, chunk);

    delete[] indexes;
}

const size_t kMinSkip = 250;
const int kMinFilterRatio = 50;

void searcher::search_lines(uint32_t *indexes, int count,
                            const chunk *chunk)
{
    log_profile("search_lines: Searching %d/%d indexes.\n", count, chunk->size);

    if (count == 0)
        return;

    if (count * kMinFilterRatio > chunk->size) {
        full_search(chunk);
        return;
    }

    {
        run_timer run(sort_time_);
        lsd_radix_sort(indexes, indexes + count);
    }


    StringPiece search((char*)chunk->data, chunk->size);
    uint32_t max = indexes[0];
    uint32_t min = line_start(chunk, indexes[0]);
    for (int i = 0; i <= count && !exit_early(); i++) {
        if (i != count) {
            if (indexes[i] < max) continue;
            if (indexes[i] < max + kMinSkip) {
                max = indexes[i];
                continue;
            }
        }

        int end = line_end(chunk, max);
        full_search(chunk, min, end);

        if (i != count) {
            max = indexes[i];
            min = line_start(chunk, max);
        }
    }
}

void searcher::full_search(const chunk *chunk)
{
    full_search(chunk, 0, chunk->size - 1);
}

void searcher::full_search(const chunk *chunk, size_t minpos, size_t maxpos)
{
    StringPiece str((char*)chunk->data, chunk->size);
    StringPiece match;
    int pos = minpos, new_pos;
    while (pos < maxpos && !exit_early()) {
        {
            run_timer run(re2_time_);
            if (!pat_.Match(str, pos, maxpos, RE2::UNANCHORED, &match, 1))
                break;
        }
        assert(memchr(match.data(), '\n', match.size()) == NULL);
        StringPiece line = find_line(str, match);
        if (utf8::is_valid(line.data(), line.data() + line.size()))
            find_match(chunk, match, line);
        new_pos = line.size() + line.data() - str.data() + 1;
        assert(new_pos > pos);
        pos = new_pos;
    }
}


match_result *searcher::try_match(const StringPiece& line,
                                  const StringPiece& match,
                                  search_file *sf) {

    int lno = 1;
    vector<StringPiece>::iterator it;
    for (it = sf->content.begin();
         it != sf->content.end(); ++it) {
        if (line.data() >= it->data() &&
            line.data() <= it->data() + it->size()) {
            lno += count(it->data(), line.data(), '\n');
            break;
        } else {
            lno += count(it->data(), it->data() + it->size(), '\n') + 1;
        }
    }

    if (it == sf->content.end())
        return 0;

    match_result *m = new match_result;
    m->file = sf;
    m->lno  = lno;
    m->line = line;
    m->matchleft = int(match.data() - line.data());
    m->matchright = m->matchleft + match.size();

    vector<StringPiece>::iterator mit = it;
    StringPiece l = line;
    int i = 0;

    for (i = 0; i < kContextLines; i++) {
        if (l.data() == it->data()) {
            if (it == sf->content.begin())
                break;
            --it;
            l = StringPiece(it->data() + it->size() + 1, 0);
        }
        l = find_line(*it, StringPiece(l.data() - 1, 0));
        m->context_before.push_back(l.as_string());
    }

    l = line;
    it = mit;
    for (i = 0; i < kContextLines; i++) {
        if (l.data() + l.size() == it->data() + it->size()) {
            if (++it == sf->content.end())
                break;
            l = StringPiece(it->data() - 1, 0);
        }
        l = find_line(*it, StringPiece(l.data() + l.size() + 1, 0));
        m->context_after.push_back(l.as_string());
    }

    return m;
}

code_searcher::search_thread::search_thread(code_searcher *cs)
    : cs_(cs), pool_(0) {
}

int code_searcher::search_thread::match(RE2& pat, match_stats *stats, exit_reason *why) {
    list<chunk*>::iterator it;
    match_result *m;
    int matches = 0;
    int pending = cs_->alloc_->size();

    assert(cs_->finalized_);
    if (!pool_)
        pool_ = new thread_pool<pair<searcher*, chunk*>,
                                bool(*)(const pair<searcher*, chunk*>&)>
        (FLAGS_threads, &search_one);

    thread_queue<match_result*> results;
    searcher search(cs_, results, pat);

    *why = kExitNone;

    if (!FLAGS_search)
        return 0;

    for (it = cs_->alloc_->begin(); it != cs_->alloc_->end(); it++) {
        pool_->queue(pair<searcher*, chunk*>(&search, *it));
    }

    while (pending) {
        m = results.pop();
        if (!m) {
            pending--;
            continue;
        }
        matches++;
        print_match(m);
        delete m;
    }

    search.get_stats(stats);
    *why = search.why();
    return matches;
}


void code_searcher::search_thread::print_match(const match_result *m) {
    if (FLAGS_quiet)
        return;
    else if (cs_->output_json_)
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

void code_searcher::search_thread::print_match_json(const match_result *m) {
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

code_searcher::search_thread::~search_thread() {
    if (pool_) {
        for (int i = 0; i < FLAGS_threads; i++)
            pool_->queue(pair<searcher*, chunk*>(0, 0));
        delete pool_;
    }
}

bool code_searcher::search_thread::search_one(const pair<searcher*, chunk*>& pair) {
    if (!pair.first)
        return true;
    (*pair.first)(pair.second);
    pair.first->queue_.push(NULL);
    return false;
}
