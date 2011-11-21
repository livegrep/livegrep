#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <locale>
#include <list>
#include <iostream>
#include <string>
#include <fstream>

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
#include "radix_sort.h"
#include "atomic.h"

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

namespace re2 {
    extern int32_t FLAGS_filtered_re2_min_atom_len;
};

const int kMaxFilters = 4;

struct search_file {
    string path;
    const char *ref;
    git_oid oid;
    int no;
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
        matches_(0), re2_time_(false), git_time_(false),
        index_time_(false), sort_time_(false)
    {
        int id;
        re2::FLAGS_filtered_re2_min_atom_len = 5;
        while(re2::FLAGS_filtered_re2_min_atom_len > 0) {
            re2::FilteredRE2 fre2;
            assert(!fre2.Add(pat.pattern(), pat.options(), &id));
            fre2.Compile(&filter_, false);
            if (filter_.size() > 0 && filter_.size() < kMaxFilters)
                break;
            re2::FLAGS_filtered_re2_min_atom_len--;
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
    void full_search(const thread_state& ts, const chunk *chunk,
                     size_t minpos, size_t maxpos);

    void filtered_search(const thread_state& ts, const chunk *chunk);
    void search_lines(uint32_t *left, int count,
                      const thread_state& ts, const chunk *chunk);

    void find_match (const chunk *chunk,
                     const StringPiece& match,
                     const StringPiece& line,
                     const thread_state& ts) {
        run_timer run(git_time_);
        timer tm;
        int off = (unsigned char*)line.data() - chunk->data;
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

    code_searcher *cc_;
    RE2& pat_;
    thread_queue<match_result*>& queue_;
    atomic_int matches_;
    vector<string> filter_;
    timer re2_time_;
    timer git_time_;
    timer index_time_;
    timer sort_time_;
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

    refs_.push_back(ref);

    walk_tree(ref, "", tree);
}

void code_searcher::dump_stats() {
    log_profile("chunk_files: %d\n", chunk::chunk_files);
    printf("Bytes: %ld (dedup: %ld)\n", stats_.bytes, stats_.dedup_bytes);
    printf("Lines: %ld (dedup: %ld)\n", stats_.lines, stats_.dedup_lines);
}

const uint32_t kIndexMagic   = 0xc0d35eac;
const uint32_t kIndexVersion = 2;

struct index_header {
    uint32_t magic;
    uint32_t version;
    uint32_t chunk_size;
    uint32_t nrefs;
    uint32_t nfiles;
    uint32_t nchunks;
} __attribute__((packed));


struct chunk_header {
    uint32_t size;
    uint32_t nfiles;
} __attribute__((packed));

static void dump_int32(ostream& stream, uint32_t i) {
    stream.write(reinterpret_cast<char*>(&i), sizeof i);
}

static void dump_string(ostream& stream, const char *str) {
    uint32_t len = strlen(str);
    dump_int32(stream, len);
    stream.write(str, len);
}

void code_searcher::dump_file(ostream& stream, search_file *sf) {
    /* (str path, int ref, oid id) */
    dump_string(stream, sf->path.c_str());
    dump_int32(stream, find(refs_.begin(), refs_.end(), sf->ref) - refs_.begin());
    stream.write(reinterpret_cast<char*>(&sf->oid), sizeof sf->oid);
}

void dump_chunk_file(ostream& stream, chunk_file *cf) {
    dump_int32(stream, cf->file->no);
    dump_int32(stream, cf->left);
    dump_int32(stream, cf->right);
}

void code_searcher::dump_chunk(ostream& stream, chunk *chunk) {
    chunk_header hdr = { uint32_t(chunk->size), uint32_t(chunk->files.size()) };
    stream.write(reinterpret_cast<char*>(&hdr), sizeof hdr);
    for (vector<chunk_file>::iterator it = chunk->files.begin();
         it != chunk->files.end(); it ++)
        dump_chunk_file(stream, &(*it));
    stream.write(reinterpret_cast<char*>(chunk->data), chunk->size);
    stream.write(reinterpret_cast<char*>(chunk->suffixes),
                 sizeof(uint32_t) * chunk->size);
}

void code_searcher::dump_index(const string& path) {
    assert(finalized_);
    ofstream stream(path.c_str());
    index_header hdr;
    hdr.magic   = kIndexMagic;
    hdr.version = kIndexVersion;
    hdr.chunk_size = kChunkSize;
    hdr.nrefs   = refs_.size();
    hdr.nfiles  = files_.size();
    hdr.nchunks = alloc_->size();

    stream.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    for (vector<const char*>::iterator it = refs_.begin();
         it != refs_.end(); ++it) {
        dump_string(stream, *it);
    }

    for (vector<search_file*>::iterator it = files_.begin();
         it != files_.end(); ++it) {
        dump_file(stream, *it);
    }

    for (list<chunk*>::iterator it = alloc_->begin();
         it != alloc_->end(); ++it) {
        dump_chunk(stream, *it);
    }
}

uint32_t load_int32(istream& stream) {
    uint32_t out;
    stream.read(reinterpret_cast<char*>(&out), sizeof out);
    return out;
}

char *load_string(istream& stream) {
    uint32_t len = load_int32(stream);
    char *buf = new char[len + 1];
    stream.read(buf, len);
    buf[len] = 0;
    return buf;
}

search_file *code_searcher::load_file(istream& stream) {
    search_file *sf = new search_file;
    char *str = load_string(stream);
    sf->path = str;
    delete[] str;
    sf->ref = refs_[load_int32(stream)];
    stream.read(reinterpret_cast<char*>(&sf->oid), sizeof(sf->oid));
    sf->no = files_.size();
    return sf;
}

void code_searcher::load_chunk_file(istream& stream, chunk_file *cf) {
    cf->file = files_[load_int32(stream)];
    cf->left = load_int32(stream);
    cf->right = load_int32(stream);
}

void code_searcher::load_chunk(istream& stream, chunk *chunk) {
    chunk_header hdr;
    stream.read(reinterpret_cast<char*>(&hdr), sizeof hdr);
    assert(hdr.size <= kChunkSpace);
    chunk->size = hdr.size;
    for (int i = 0; i < hdr.nfiles; i++) {
        chunk->files.push_back(chunk_file());
        load_chunk_file(stream, &chunk->files.back());
    }
    stream.read(reinterpret_cast<char*>(chunk->data), chunk->size);
    chunk->suffixes = new uint32_t[chunk->size];
    stream.read(reinterpret_cast<char*>(chunk->suffixes),
                sizeof(uint32_t) * chunk->size);
}

void code_searcher::load_index(const string& path) {
    assert(!finalized_);
    assert(!refs_.size());

    ifstream stream(path.c_str());
    index_header hdr;
    stream.read(reinterpret_cast<char*>(&hdr), sizeof hdr);
    assert(!stream.fail());
    assert(hdr.magic == kIndexMagic);
    assert(hdr.version == kIndexVersion);
    assert(hdr.chunk_size == kChunkSize);

    for (int i = 0; i < hdr.nrefs; i++) {
        refs_.push_back(load_string(stream));
    }

    for (int i = 0; i < hdr.nfiles; i++) {
        files_.push_back(load_file(stream));
    }

    for (int i = 0; i < hdr.nchunks; i++) {
        load_chunk(stream, alloc_->current_chunk());
        if (i != hdr.nchunks - 1)
            alloc_->skip_chunk();
    }

    finalized_ = true;
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

    if (FLAGS_index && filter_.size() > 0 && filter_.size() < kMaxFilters)
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

    pair<uint32_t*, uint32_t*> ranges[kMaxFilters];
    uint32_t *indexes;
    int count = 0, off = 0;

    {
        run_timer run(index_time_);
        for (int i = 0; i < filter_.size(); i++) {
            ranges[i] = equal_range(chunk->suffixes,
                                    chunk->suffixes + chunk->size,
                                    filter_[i], lt);
            count += ranges[i].second - ranges[i].first;
        }
        indexes = new uint32_t[count];
        for (int i = 0; i < filter_.size(); i++) {
            int width = ranges[i].second - ranges[i].first;
            memcpy(&indexes[off], ranges[i].first, width * sizeof(uint32_t));
            off += width;
        }
    }

    search_lines(indexes, count, ts, chunk);
    delete[] indexes;
}

const size_t kMinSkip = 250;

void searcher::search_lines(uint32_t *indexes, int count,
                            const thread_state& ts,
                            const chunk *chunk)
{
    log_profile("search_lines: Searching %d/%d indexes.\n", count, chunk->size);

    if (count == 0)
        return;

    {
        run_timer run(sort_time_);
        lsd_radix_sort(indexes, indexes + count);
    }


    StringPiece search((char*)chunk->data, chunk->size);
    uint32_t max = indexes[0];
    uint32_t min = line_start(chunk, indexes[0]);
    for (int i = 0; i <= count; i++) {
        if (i != count) {
            if (indexes[i] < max) continue;
            if (indexes[i] < max + kMinSkip) {
                max = indexes[i];
                continue;
            }
        }

        int end = line_end(chunk, max);
        full_search(ts, chunk, min, end);

        if (i != count) {
            max = indexes[i];
            min = line_start(chunk, max);
        }
    }
}

void searcher::full_search(const thread_state& ts, const chunk *chunk)
{
    full_search(ts, chunk, 0, chunk->size - 1);
}

void searcher::full_search(const thread_state& ts, const chunk *chunk,
                           size_t minpos, size_t maxpos)
{
    StringPiece str((char*)chunk->data, chunk->size);
    StringPiece match;
    int pos = minpos, new_pos;
    while (pos < maxpos && matches_.load() < kMaxMatches) {
        {
            run_timer run(re2_time_);
            if (!pat_.Match(str, pos, maxpos, RE2::UNANCHORED, &match, 1))
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
