/********************************************************************
 * livegrep -- codesearch.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include <locale>
#include <list>
#include <iostream>
#include <string>
#include <fstream>
#include <limits>
#include <atomic>

#include "src/lib/timer.h"
#include "src/lib/metrics.h"
#include "src/lib/thread_queue.h"
#include "src/lib/radix_sort.h"
#include "src/lib/per_thread.h"
#include "src/lib/debug.h"

#include "src/codesearch.h"
#include "src/chunk.h"
#include "src/chunk_allocator.h"
#include "src/query_planner.h"
#include "src/content.h"

#include "absl/strings/string_view.h"

#include "divsufsort.h"
#include "re2/re2.h"
#include "gflags/gflags.h"

#include "utf8.h"

using re2::RE2;
using re2::StringPiece;
using namespace std;

const size_t kMinSkip = 250;
const int kMinFilterRatio = 50;
const int kMaxScan        = (1 << 20);

DEFINE_bool(index, true, "Create a suffix-array index to speed searches.");
DEFINE_bool(compress, true, "Compress file contents linewise");
DEFINE_bool(drop_cache, false, "Drop caches before each search");
DEFINE_bool(search, true, "Actually do the search.");
DEFINE_int32(timeout, 1000, "The number of milliseconds a single search may run for.");
DEFINE_int32(threads, 4, "Number of threads to use.");
DEFINE_int32(line_limit, 1024, "Maximum line length to index.");

namespace {
    metric idx_bytes("index.bytes");
    metric idx_bytes_dedup("index.bytes.dedup");
    metric idx_files("index.files");
    metric idx_lines("index.lines");
    metric idx_lines_dedup("index.lines.dedup");
    metric idx_data_chunks("index.data.chunks");
    metric idx_content_chunks("index.content.chunks");
    metric idx_content_ranges("index.content.ranges");
};

#ifdef __APPLE__
/*
 * Reverse memchr()
 * Find the last occurrence of 'c' in the buffer 's' of size 'n'.
 */
void *
memrchr(const void *s,
        int c,
        size_t n)
{
    const unsigned char *cp;

    if (n != 0) {
	cp = (unsigned char *)s + n;
	do {
	    if (*(--cp) == (unsigned char)c)
		return (void *)cp;
	} while (--n != 0);
    }
    return (void *)0;
}
#endif

size_t hashstr::operator()(const StringPiece& str) const {
    return absl::Hash<absl::string_view>{}(absl::string_view(str.data(), str.size()));
}

const StringPiece empty_string(NULL, 0);

class search_limiter {
public:
    search_limiter(int query_max_matches) : matches_(0), max_matches_(query_max_matches), exit_reason_(kExitNone) {
        if (FLAGS_timeout <= 0) {
            deadline_.tv_sec = numeric_limits<time_t>::max();
        } else {
            timeval timeout = {
                0, FLAGS_timeout * 1000
            }, now;
            gettimeofday(&now, NULL);
            timeval_add(&deadline_, &now, &timeout);
        }
    }

    exit_reason why() {
        return exit_reason_;
    }

    bool exit_early() {
        if (exit_reason_)
            return true;

#ifdef CODESEARCH_SLOWGTOD
        static int counter = 1000;
        if (--counter)
            return false;
        counter = 1000;
#endif
        timeval now;
        gettimeofday(&now, NULL);
        if (now.tv_sec > deadline_.tv_sec ||
            (now.tv_sec == deadline_.tv_sec && now.tv_usec > deadline_.tv_usec)) {
            exit_reason_ = kExitTimeout;
            return true;
        }
        return false;
    }

    void record_match() {
        int matches = ++matches_;
        if (exit_reason_)
            return;
        if (max_matches_ && matches >= max_matches_) {
            exit_reason_ = kExitMatchLimit;
        }
    }

protected:
    atomic_int matches_;
    int max_matches_;
    timeval deadline_;
    exit_reason exit_reason_;
};

class code_searcher;
struct match_finger;

bool accept(const query *q, const indexed_file *file) {
    if (q->file_pat &&
        !q->file_pat->Match(file->path, 0, file->path.size(),
                            RE2::UNANCHORED, 0, 0))
        return false;

    if (q->tree_pat &&
        !q->tree_pat->Match(file->tree->name, 0,
                            file->tree->name.size(),
                            RE2::UNANCHORED, 0, 0))
        return false;

    if (q->negate.file_pat &&
        q->negate.file_pat->Match(file->path, 0,
                                  file->path.size(),
                                  RE2::UNANCHORED, 0, 0))
        return false;

    if (q->negate.tree_pat &&
        q->negate.tree_pat->Match(file->tree->name, 0,
                                  file->tree->name.size(),
                                  RE2::UNANCHORED, 0, 0))
        return false;

    return true;
}

bool accept(const query *q, const list<indexed_file *> &sfs) {
    for (list<indexed_file *>::const_iterator it = sfs.begin();
         it != sfs.end(); ++it) {
        if (accept(q, *it))
            return true;
    }
    return false;
}

class searcher {
public:
    searcher(const code_searcher *cc,
             const query &q,
             const intrusive_ptr<QueryPlan> index_key,
             const code_searcher::search_thread::transform_func& func) :
        cc_(cc), query_(&q), transform_(func), queue_(),
        limiter_(q.max_matches), index_key_(index_key), re2_time_(false),
        git_time_(false), index_time_(false), sort_time_(false),
        analyze_time_(false), files_(cc->files_.size(), 0xff),
        files_density_(-1)
    {}

    ~searcher() {
        debug(kDebugProfile, "re2 time: %d.%06ds",
              int(re2_time_.elapsed().tv_sec),
              int(re2_time_.elapsed().tv_usec));
        debug(kDebugProfile, "git time: %d.%06ds",
              int(git_time_.elapsed().tv_sec),
              int(git_time_.elapsed().tv_usec));
        debug(kDebugProfile, "index time: %d.%06ds",
              int(index_time_.elapsed().tv_sec),
              int(index_time_.elapsed().tv_usec));
        debug(kDebugProfile, "sort time: %d.%06ds",
              int(sort_time_.elapsed().tv_sec),
              int(sort_time_.elapsed().tv_usec));
    }

    void operator()(const chunk *chunk);

    void get_stats(match_stats *stats) {
        struct timeval t;

        t = re2_time_.elapsed();
        timeradd(&stats->re2_time, &t, &stats->re2_time);

        t = git_time_.elapsed();
        timeradd(&stats->git_time, &t, &stats->git_time);

        t = index_time_.elapsed();
        timeradd(&stats->index_time, &t, &stats->index_time);

        t = sort_time_.elapsed();
        timeradd(&stats->sort_time , &t, &stats->sort_time);

        t = analyze_time_.elapsed();
        timeradd(&stats->analyze_time, &t, &stats->analyze_time);
    }

    exit_reason why() {
        return limiter_.why();
    }

protected:
    void next_range(match_finger *finger, int& minpos, int& maxpos, int end);
    bool should_search_chunk(const chunk *chunk);
    void full_search(const chunk *chunk);
    void full_search(match_finger *finger, const chunk *chunk,
                     size_t minpos, size_t maxpos);

    void filtered_search(const chunk *chunk);
    void search_lines(uint32_t *left, int count, const chunk *chunk);

    double files_density(void) {
        std::unique_lock<std::mutex> locked(mtx_);
        if (files_density_ >= 0)
            return files_density_;

        int hits = 0;
        int sample = min(1000, int(cc_->files_.size()));
        for (int i = 0; i < sample; i++) {
            if (accept(query_, cc_->files_[rand() % cc_->files_.size()].get()))
                hits++;
        }
        return (files_density_ = double(hits) / sample);
    }

    /*
     * Do a linear walk over chunk->files, searching for all files
p     * which contain `match', which is contained within `line'.
     */
    void find_match_brute(const chunk *chunk,
                          const StringPiece& match,
                          const StringPiece& line);

    /*
     * Given a match `match', contained within `line', find all files
     * that contain that match. If indexing is enabled, do this by
     * walking the chunk_file BST; Otherwise, fall back on a
     * brute-force linear walk.
     */
    void find_match(const chunk *chunk,
                    const StringPiece& match,
                    const StringPiece& line);

    /*
     * Given a matching substring, its containing line, and a search
     * file, determine whether that file actually contains that line,
     * and if so, post results to queue_
     */
    void try_match(const StringPiece&,
                   const StringPiece&,
                   indexed_file *);

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

    const code_searcher *cc_;
    const query *query_;
    const code_searcher::search_thread::transform_func transform_;
    thread_queue<match_result*> queue_;
    search_limiter limiter_;
    intrusive_ptr<QueryPlan> index_key_;
    timer re2_time_;
    timer git_time_;
    timer index_time_;
    timer sort_time_;
    timer analyze_time_;
    vector<uint8_t> files_;

    /*
     * The approximate ratio of how many files match file_pat and
     * tree_pat. Lazily computed -- -1 means it hasn't been computed
     * yet. Protected by mtx_.
     */
    double files_density_;
    std::mutex mtx_;

    friend class code_searcher::search_thread;
};

class filename_searcher {
public:
    filename_searcher(const code_searcher *cc,
                      const query &q,
                      intrusive_ptr<QueryPlan> index_key) :
        cc_(cc), query_(&q), index_key_(index_key), queue_(), limiter_(q.max_matches)
    {}

    void operator()();

    exit_reason why() {
        return limiter_.why();
    }

protected:
    void match_filename(indexed_file *file);

    const code_searcher *cc_;
    const query *query_;
    intrusive_ptr<QueryPlan> index_key_;
    thread_queue<file_result*> queue_;
    search_limiter limiter_;

    friend class code_searcher::search_thread;
};

int suffix_search(const unsigned char *data,
                  const uint32_t *suffixes,
                  int size,
                  intrusive_ptr<QueryPlan> index,
                  vector<uint32_t> &indexes_out);

void filename_searcher::operator()()
{
    static per_thread<vector<uint32_t> > indexes;
    if (!indexes.get()) {
        indexes.put(new vector<uint32_t>(cc_->filename_data_.size() / kMinFilterRatio / 10));
    }

    int count = suffix_search(cc_->filename_data_.data(),
                              cc_->filename_suffixes_.data(),
                              cc_->filename_data_.size(), index_key_, *indexes);

    if (count > indexes->size()) {
        for (auto it = cc_->files_.begin(); it < cc_->files_.end(); it++) {
            if (limiter_.exit_early()) {
                return;
            }
            match_filename(it->get());
        }
        return;
    }

    lsd_radix_sort(indexes->data(), indexes->data() + count);

    // find candidate indexed_files from the positions of the candidate matches.
    // This is O(candidate_matches * log(indexed_files)), but it could probably
    // be done more cleverly in something like O(candidate_matches + log(indexed_files))

    // moving the left bound as we go isn't a big-O improvement, but may help a little bit.
    auto left_bound = cc_->filename_positions_.begin();
    int previous_first = -1;

    for (int i = 0; i < count; i++) {
        if (limiter_.exit_early()) {
            break;
        }

        int target_index = (*indexes)[i];
        pair<int, indexed_file*> target(target_index, NULL);
        auto lb = lower_bound(left_bound, cc_->filename_positions_.end(), target);

        if (lb->first == previous_first) {
            // We have already returned this filename because of a match
            // earlier in its text.
            continue;
        }
        previous_first = lb->first;

        if (lb->first != target_index) {
            assert(lb == cc_->filename_positions_.end() ||
                   lb->first > target_index);
            assert(lb != left_bound);
            lb--;
        }
        assert(lb->first <= (*indexes)[i]);
        assert((*indexes)[i] < lb->first + lb->second->path.size());
        match_filename(lb->second);

        left_bound = lb;
    }
}

void filename_searcher::match_filename(indexed_file *file) {
    if (!accept(query_, file))
        return;

    StringPiece filepath = StringPiece(file->path);
    StringPiece match;
    if (!query_->line_pat->Match(filepath, 0, filepath.size(),
                                 RE2::UNANCHORED, &match, 1))
        return;

    file_result *f = new file_result;
    f->file = file;
    f->matchleft = utf8::distance(filepath.data(), match.data());
    f->matchright = f->matchleft + utf8::distance(match.data(), match.data() + match.size());

    queue_.push(f);
    limiter_.record_match();
}

code_searcher::code_searcher()
    : alloc_(), finalized_(false), filename_data_(), filename_suffixes_()
{
}

void code_searcher::set_alloc(std::unique_ptr<chunk_allocator> alloc) {
    assert(!alloc_);
    alloc_ = move(alloc);
}

code_searcher::~code_searcher() {
    if (alloc_)
        alloc_->cleanup();
}

void code_searcher::index_filenames() {
    log("Building filename index...");
    filename_positions_.reserve(files_.size());

    size_t filename_data_size = 0;
    for (auto it = files_.begin(); it != files_.end(); ++it) {
        filename_data_size += (*it)->path.size() + 1;
    }

    filename_data_.resize(filename_data_size);
    int offset = 0;
    for (auto it = files_.begin(); it != files_.end(); ++it) {
        memcpy(filename_data_.data() + offset, (*it)->path.data(), (*it)->path.size());
        filename_data_[offset + (*it)->path.size()] = '\0';
        filename_positions_.emplace_back(offset, it->get());
        offset += (*it)->path.size() + 1;
    }

    filename_suffixes_.resize(filename_data_size);
    divsufsort(filename_data_.data(),
               reinterpret_cast<saidx_t*>(filename_suffixes_.data()),
               filename_data_size);
}

void code_searcher::finalize() {
    assert(!finalized_);
    finalized_ = true;
    index_filenames();
    alloc_->finalize();

    timeval now;
    gettimeofday(&now, NULL);
    index_timestamp_ = now.tv_sec;

    idx_data_chunks.inc(alloc_->end() - alloc_->begin());
    idx_content_chunks.inc(alloc_->end_content() - alloc_->begin_content());
}

vector<indexed_tree> code_searcher::trees() const {
    vector<indexed_tree> out;
    out.reserve(trees_.size());
    for (auto it = trees_.begin(); it != trees_.end(); ++it)
        out.push_back(**it);
    return out;
}

const indexed_tree* code_searcher::open_tree(const string &name,
                                             const Metadata &metadata,
                                             const string &version) {
    auto tree = std::make_unique<indexed_tree>();
    tree->name = name;
    tree->version = version;
    tree->metadata = metadata;
    trees_.push_back(move(tree));
    return trees_.back().get();
}

const indexed_tree* code_searcher::open_tree(const string &name,
                                             const string &version) {
    return open_tree(name, Metadata(), version);
}

void code_searcher::index_file(const indexed_tree *tree,
                               const string& path,
                               StringPiece contents) {
    assert(!finalized_);
    assert(alloc_);
    size_t len = contents.size();
    const char *p = contents.data();
    const char *end = p + len;
    const char *f;
    chunk *c;
    chunk *prev = NULL;
    StringPiece line;

    if (memchr(p, 0, len) != NULL)
        return;

    idx_bytes.inc(len);
    idx_files.inc();

    auto file = std::make_unique<indexed_file>();
    file->tree = tree;
    file->path = path;
    file->no  = files_.size();
    auto *sf = file.get();
    files_.push_back(move(file));

    uint32_t lines = count(p, end, '\n');

    file_contents_builder content;

    while ((f = static_cast<const char*>(memchr(p, '\n', end - p))) != 0) {
    final:
        idx_lines.inc();
        if (f - p + 1 >= FLAGS_line_limit) {
            // Don't index the long line, but do index an empty
            // line so that line number of future lines are
            // preserved.
            p = f;
        }
        decltype(lines_)::iterator it = lines_.end();
        if (FLAGS_compress) {
            it = lines_.find(StringPiece(p, f - p));
        }
        if (it == lines_.end()) {
            idx_bytes_dedup.inc((f - p) + 1);
            idx_lines_dedup.inc();

            unsigned char *alloc = alloc_->alloc(f - p + 1);
            memcpy(alloc, p, f - p);
            alloc[f - p] = '\n';
            line = StringPiece((char*)alloc, f - p);
            if (FLAGS_compress) {
                if (alloc_->current_chunk() != prev)
                    lines_.clear();
                lines_.insert(line);
            }
            prev = c = alloc_->current_chunk();
        } else {
            line = *it;
            c = alloc_->chunk_from_string
                (reinterpret_cast<const unsigned char*>(line.data()));
        }
        {
            c->add_chunk_file(sf, line);
        }
        content.extend(c, line);
        p = min(end, f + 1);
    }
    if (p < end - 1) {
        // Handle files with no trailing newline by jumping back and
        // adding the final line.
        assert(*(end-1) != '\n');
        f = end;
        lines++;
        goto final;
    }

    sf->content = content.build(alloc_.get());
    if (sf->content == 0) {
        fprintf(stderr, "WARN: %s:%s:%s is too large to be indexed.\n",
                tree->name.c_str(), tree->version.c_str(), path.c_str());
        file_contents_builder dummy;
        sf->content = dummy.build(alloc_.get());
    }
    idx_content_ranges.inc(sf->content->size());
    assert(sf->content->size() <= 3*lines);

    for (auto it = alloc_->begin();
         it != alloc_->end(); it++) {
        (*it)->finish_file();
    }
}

bool searcher::should_search_chunk(const chunk *chunk) {
    if (!query_->tree_pat) {
        return true;
    }

    // skip chunks that don't contain any repos we're looking for
    for (auto it = chunk->tree_names.begin(); it != chunk->tree_names.end(); it++) {
        if (query_->tree_pat->Match(*it, 0,
                                    it->size(),
                                    RE2::UNANCHORED, 0, 0)) {
            return true;
        }
    }
    return false;
}

void searcher::operator()(const chunk *chunk)
{
    if (limiter_.exit_early())
        return;

    if (!should_search_chunk(chunk))
        return;

    if (FLAGS_index && index_key_ && !index_key_->empty())
        filtered_search(chunk);
    else
        full_search(chunk);
}

struct walk_state {
    const uint32_t *left, *right;
    intrusive_ptr<QueryPlan> key;
    int depth;
};

struct lt_index {
    const unsigned char *data_;
    int idx_;

    bool operator()(uint32_t lhs, unsigned char rhs) {
        return cmp(lhs, rhs) < 0;
    }

    bool operator()(unsigned char lhs, uint32_t rhs) {
        return cmp(rhs, lhs) > 0;
    }

    int cmp(uint32_t lhs, unsigned char rhs) {
        unsigned char lc = data_[lhs + idx_];
        if (lc == '\n')
            return -1;
        return (int)lc - (int)rhs;
    }
};

int suffix_search(const unsigned char *data,
                  const uint32_t *suffixes,
                  int size,
                  intrusive_ptr<QueryPlan> index,
                  vector<uint32_t> &indexes_out) {
    int count = 0;
    vector<walk_state> stack;
    stack.push_back((walk_state){
            suffixes, suffixes + size, index, 0});

    while (!stack.empty()) {
        walk_state st = stack.back();
        stack.pop_back();
        if (!st.key || st.key->empty() || (st.right - st.left) <= 100) {
            if ((count + st.right - st.left) > indexes_out.size()) {
                count = indexes_out.size() + 1;
                break;
            }
            memcpy(&indexes_out[count], st.left,
                   (st.right - st.left) * sizeof(uint32_t));
            count += (st.right - st.left);
            continue;
        }
        lt_index lt = {data, st.depth};
        for (QueryPlan::iterator it = st.key->begin();
             it != st.key->end(); ++it) {
            const uint32_t *l, *r;
            l = lower_bound(st.left, st.right, it->first.first, lt);
            const uint32_t *right = lower_bound(l, st.right,
                                          (unsigned char)(it->first.second + 1),
                                          lt);
            if (l == right)
                continue;

            if (st.depth)
                assert(data[*l + st.depth - 1] ==
                       data[*(right - 1) + st.depth - 1]);

            assert(l == st.left ||
                   data[*(l-1) + st.depth] == '\n' ||
                   data[*(l-1) + st.depth] < it->first.first);
            assert(data[*l + st.depth] >= it->first.first);
            assert(right == st.right ||
                   data[*right + st.depth] > it->first.second);

            for (unsigned char ch = it->first.first; ch <= it->first.second;
                 ch++, l = r) {
                r = lower_bound(l, right, (unsigned char)(ch + 1), lt);

                if (r != l) {
                    stack.push_back((walk_state){l, r, it->second, st.depth + 1});
                }
            }
        }
    }
    return count;
}

void searcher::filtered_search(const chunk *chunk)
{
    static per_thread<vector<uint32_t> > indexes;
    if (!indexes.get()) {
        indexes.put(new vector<uint32_t>(cc_->alloc_->chunk_size() / kMinFilterRatio));
    }
    int count;
    {
        run_timer run(index_time_);
        count = suffix_search(chunk->data, chunk->suffixes, chunk->size, index_key_, *indexes);
    }

    search_lines(&(*indexes)[0], count, chunk);
}

struct match_finger {
    const chunk *chunk_;
    vector<chunk_file>::const_iterator it_;
    match_finger(const chunk *chunk) :
        chunk_(chunk), it_(chunk->files.begin()) {};
};

void searcher::search_lines(uint32_t *indexes, int count,
                            const chunk *chunk)
{
    debug(kDebugProfile, "search_lines: Searching %d/%d indexes.", count, chunk->size);

    if (count == 0)
        return;

    if (count * kMinFilterRatio > chunk->size) {
        full_search(chunk);
        return;
    }

    if ((query_->file_pat || query_->tree_pat) &&
        double(count * 30) / chunk->size > files_density()) {
        full_search(chunk);
        return;
    }

    {
        run_timer run(sort_time_);
        lsd_radix_sort(indexes, indexes + count);
    }

    match_finger finger(chunk);

    StringPiece search((char*)chunk->data, chunk->size);
    uint32_t max = indexes[0];
    uint32_t min = line_start(chunk, indexes[0]);
    for (int i = 0; i <= count && !limiter_.exit_early(); i++) {
        if (i != count) {
            if (indexes[i] < max) continue;
            if (indexes[i] < max + kMinSkip) {
                max = indexes[i];
                continue;
            }
        }

        int end = line_end(chunk, max);
        full_search(&finger, chunk, min, end);

        if (i != count) {
            max = indexes[i];
            min = line_start(chunk, max);
        }
    }
}

void searcher::full_search(const chunk *chunk)
{
    match_finger finger(chunk);
    full_search(&finger, chunk, 0, chunk->size - 1);
}

void searcher::next_range(match_finger *finger,
                          int& pos, int& endpos, int maxpos)
{
    if ((!query_->file_pat && !query_->tree_pat) || !FLAGS_index)
        return;

    debug(kDebugSearch, "next_range(%d, %d, %d)", pos, endpos, maxpos);

    vector<chunk_file>::const_iterator& it = finger->it_;
    const vector<chunk_file>::const_iterator& end = finger->chunk_->files.end();

    /* Find the first matching range that intersects [pos, maxpos) */
    while (it != end &&
           (it->right < pos || !accept(query_, it->files)) &&
           it->left < maxpos)
        ++it;

    if (it == end || it->left >= maxpos) {
        pos = endpos = maxpos;
        return;
    }

    pos    = max(pos, it->left);
    endpos = it->right;

    /*
     * Now scan until we either:
     * - prove that [pos, maxpos) is all in range,
     * - find a gap greater than kMinSkip, or
     * - pass maxpos entirely.
     */
    do {
        if (it->left >= endpos + kMinSkip)
            break;
        if (it->right >= endpos && accept(query_, it->files)) {
            endpos = max(endpos, it->right);
            if (endpos >= maxpos)
                /*
                 * We've accepted the entire range. No point in going on.
                 */
                break;
        }
        ++it;
    } while (it != end && it->left < maxpos);

    endpos = min(endpos, maxpos);
}

void searcher::full_search(match_finger *finger,
                           const chunk *chunk, size_t minpos, size_t maxpos)
{
    StringPiece str((char*)chunk->data, chunk->size);
    StringPiece match;
    int pos = minpos, new_pos, end = minpos;
    while (pos < maxpos && !limiter_.exit_early()) {
        if (pos >= end) {
            end = maxpos;
            next_range(finger, pos, end, maxpos);
            assert(pos <= end);
        }
        if (pos >= maxpos)
            break;

        debug(kDebugSearch, "[%p] range:%d-%d/%d-%d",
              (void*)(chunk), pos, end, int(minpos), int(maxpos));

        {
            int limit = end;
            if (limit - pos > kMaxScan)
                limit = line_end(chunk, pos + kMaxScan);
            run_timer run(re2_time_);
            if (!query_->line_pat->Match(str, pos, limit, RE2::UNANCHORED, &match, 1)) {
                pos = limit + 1;
                continue;
            }
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

void searcher::find_match_brute(const chunk *chunk,
                                const StringPiece& match,
                                const StringPiece& line) {
    run_timer run(git_time_);
    timer tm;
    int off = (unsigned char*)line.data() - chunk->data;
    int searched = 0;

    for(vector<chunk_file>::const_iterator it = chunk->files.begin();
        it != chunk->files.end(); it++) {
        if (off >= it->left && off <= it->right) {
            for (list<indexed_file *>::const_iterator fit = it->files.begin();
                 fit != it->files.end(); ++fit) {
                if (!accept(query_, *fit))
                    continue;
                searched++;
                if (limiter_.exit_early())
                    break;
                try_match(line, match, *fit);
            }
        }
    }

    tm.pause();
    debug(kDebugProfile, "Searched %d files in %d.%06ds",
          searched,
          int(tm.elapsed().tv_sec),
          int(tm.elapsed().tv_usec));
}

void searcher::find_match(const chunk *chunk,
                          const StringPiece& match,
                          const StringPiece& line) {
    if (!FLAGS_index) {
        find_match_brute(chunk, match, line);
        return;
    }

    run_timer run(git_time_);
    int loff = (unsigned char*)line.data() - chunk->data;

    vector<chunk_file_node *> stack;
    assert(chunk->cf_root);
    stack.push_back(chunk->cf_root.get());

    debug(kDebugSearch, "find_match(%d)", loff);

    while (!stack.empty() && !limiter_.exit_early()) {
        chunk_file_node *n = stack.back();
        stack.pop_back();

        debug(kDebugSearch,
              "walk <%d-%d> - %d", n->chunk->left, n->chunk->right,
              n->right_limit);

        if (loff > n->right_limit)
            continue;
        if (loff >= n->chunk->left) {
            if (n->right)
                stack.push_back(n->right.get());
            if (loff <= n->chunk->right) {
                debug(kDebugSearch, "visit <%d-%d>", n->chunk->left, n->chunk->right);
                assert(loff >= n->chunk->left && loff <= n->chunk->right);
                for (list<indexed_file *>::const_iterator it = n->chunk->files.begin();
                     it != n->chunk->files.end(); ++it) {
                    if (!accept(query_, *it))
                        continue;
                    if (limiter_.exit_early())
                        break;
                    try_match(line, match, *it);
                }
            }
        }
        if (n->left)
            stack.push_back(n->left.get());
    }
}


void searcher::try_match(const StringPiece& line,
                         const StringPiece& match,
                         indexed_file *sf) {

    int lno = 1;
    auto it = sf->content->begin(cc_->alloc_.get());

    while (true) {
        for (;it != sf->content->end(cc_->alloc_.get()); ++it) {
            if (line.data() >= it->data() &&
                line.data() <= it->data() + it->size()) {
                lno += count(it->data(), line.data(), '\n');
                break;
            } else {
                lno += count(it->data(), it->data() + it->size(), '\n') + 1;
            }
        }

        debug(kDebugSearch, "found match on %s:%d", sf->path.c_str(), lno);

        if (it == sf->content->end(cc_->alloc_.get()))
            return;

        match_result *m = new match_result;
        m->file = sf;
        m->lno  = lno;
        m->line = line;
        m->matchleft = utf8::distance(line.data(), match.data());
        m->matchright = m->matchleft +
            utf8::distance(match.data(), match.data() + match.size());

        // iterators for forward and backward context
        auto fit = it, bit = it;
        StringPiece l = line;
        int i = 0;

        for (i = 0; i < query_->context_lines; i++) {
            if (l.data() == bit->data()) {
                if (bit == sf->content->begin(cc_->alloc_.get()))
                    break;
                --bit;
                l = StringPiece(bit->data() + bit->size() + 1, 0);
            }
            l = find_line(*bit, StringPiece(l.data() - 1, 0));
            m->context_before.push_back(l);
        }

        l = line;

        for (i = 0; i < query_->context_lines; i++) {
            if (l.data() + l.size() == fit->data() + fit->size()) {
                if (++fit == sf->content->end(cc_->alloc_.get()))
                    break;
                l = StringPiece(fit->data() - 1, 0);
            }
            l = find_line(*fit, StringPiece(l.data() + l.size() + 1, 0));
            m->context_after.push_back(l);
        }

        if (!transform_ || transform_(m)) {
            queue_.push(m);
            limiter_.record_match();
        }
        if (limiter_.exit_early())
            break;

        ++it;
        ++lno;
    }
}

code_searcher::search_thread::search_thread(code_searcher *cs)
    : cs_(cs) {
    if (FLAGS_search) {
        for (int i = 0; i < FLAGS_threads; ++i) {
            threads_.emplace_back(search_one, this);
        }
        threads_.emplace_back(search_file_one, this);
    }
}

void code_searcher::search_thread::match(const query &q,
                                         const callback_func& cb,
                                         const file_callback_func& fcb,
                                         const transform_func& func,
                                         match_stats *stats) {
    match_result *m;
    file_result *f;
    int matches = 0, file_matches = 0;

    assert(cs_->finalized_);

    if (!FLAGS_search) {
        return;
    }

    if (FLAGS_drop_cache) {
        cs_->alloc_->drop_caches();
    }

    timer analyze_time(false);
    intrusive_ptr<QueryPlan> index_key;
    {
        run_timer run(analyze_time);
        index_key = constructQueryPlan(*q.line_pat);
    }
    debug(kDebugProfile, "analyze time: %d.%06ds",
          int(analyze_time.elapsed().tv_sec),
          int(analyze_time.elapsed().tv_usec));

    searcher search(cs_, q, index_key, func);
    filename_searcher file_search(cs_, q, index_key);
    job j;
    j.trace_id = current_trace_id();
    j.search = &search;
    j.file_search = &file_search;
    j.pending = 0;

    if (!q.filename_only) {
        for (int i = 0; i < FLAGS_threads; ++i) {
            ++j.pending;
            queue_.push(&j);
        }

        for (auto it = cs_->alloc_->begin(); it != cs_->alloc_->end(); it++) {
            j.chunks.push(*it);
        }
        j.chunks.close();
    }

    file_queue_.push(&j);

    if (!q.filename_only) {
        while (search.queue_.pop(&m)) {
            matches++;
            cb(m);
            delete m;
        }
    }

    while (file_search.queue_.pop(&f)) {
        file_matches++;
        fcb(f);
        delete f;
    }

    if (q.filename_only) {
        stats->why = file_search.why();
        stats->matches += file_matches;
    } else {
        search.get_stats(stats);
        stats->why = search.why();
        stats->matches += matches;
    }

    struct timeval t = analyze_time.elapsed();
    timeradd(&stats->analyze_time, &t, &stats->analyze_time);
}


code_searcher::search_thread::~search_thread() {
    queue_.close();
    file_queue_.close();
    for (auto it = threads_.begin(); it != threads_.end(); ++it)
        it->join();
}

void code_searcher::search_thread::search_one(search_thread *me) {
    job *j;
    while (me->queue_.pop(&j)) {
        scoped_trace_id trace(j->trace_id);

        chunk *c;
        while (j->chunks.pop(&c)) {
            (*j->search)(c);
        }

        if (--j->pending == 0)
            j->search->queue_.close();
    }
}

void code_searcher::search_thread::search_file_one(search_thread *me) {
    job *j;
    while (me->file_queue_.pop(&j)) {
        scoped_trace_id trace(j->trace_id);
        (*j->file_search)();
        j->file_search->queue_.close();
    }
}

void default_re2_options(RE2::Options &opts) {
    opts.set_never_nl(true);
    opts.set_one_line(false);
    opts.set_perl_classes(true);
    opts.set_word_boundary(true);
    opts.set_posix_syntax(true);
    opts.set_word_boundary(true);
    opts.set_log_errors(false);
}
