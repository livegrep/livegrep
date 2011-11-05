#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <locale>
#include <list>
#include <iostream>
#include <string>
#include <atomic>

#include <re2/re2.h>

#include "timer.h"
#include "thread_queue.h"
#include "thread_pool.h"
#include "codesearch.h"

#include "utf8.h"

using re2::RE2;
using re2::StringPiece;
using namespace std;

const size_t kChunkSize    = 1 << 20;
const size_t kMaxGap       = 1 << 10;
const int    kMaxMatches   = 50;
const int    kContextLines = 3;

#ifdef PROFILE_CODESEARCH
#define log_profile(format, ...) fprintf(stderr, format, __VA_ARGS__)
#else
#define log_profile(...)
#endif

struct search_file {
    string path;
    const char *ref;
    git_oid oid;
};

struct chunk_file {
    search_file *file;
    int left;
    int right;
    void expand(int l, int r) {
        left  = min(left, l);
        right = max(right, r);
    }

    bool operator<(const chunk_file& rhs) const {
        return left < rhs.left;
    }
};

struct match_result {
    search_file *file;
    int lno;
    vector<string> context_before;
    vector<string> context_after;
    StringPiece line;
    int matchleft, matchright;
};

#define CHUNK_MAGIC 0xC407FADE

struct chunk {
    static int chunk_files;
    int size;
    unsigned magic;
    vector<chunk_file> files;
    vector<chunk_file> cur_file;
    char data[0];

    chunk()
        : size(0), magic(CHUNK_MAGIC), files() {
    }

    void add_chunk_file(search_file *sf, const StringPiece& line) {
        int l = line.data() - data;
        int r = l + line.size();
        chunk_file *f = NULL;
        int min_dist = numeric_limits<int>::max(), dist;
        for (vector<chunk_file>::iterator it = cur_file.begin();
             it != cur_file.end(); it ++) {
            if (l <= it->left)
                dist = max(0, it->left - r);
            else if (r >= it->right)
                dist = max(0, l - it->right);
            else
                dist = 0;
            assert(dist == 0 || r < it->left || l > it->right);
            if (dist < min_dist) {
                min_dist = dist;
                f = &(*it);
            }
        }
        if (f && min_dist < kMaxGap) {
            f->expand(l, r);
            return;
        }
        chunk_files++;
        cur_file.push_back(chunk_file());
        chunk_file& cf = cur_file.back();
        cf.file = sf;
        cf.left = l;
        cf.right = r;
    }

    void finish_file() {
        int right = -1;
        sort(cur_file.begin(), cur_file.end());
        for (vector<chunk_file>::iterator it = cur_file.begin();
             it != cur_file.end(); it ++) {
            assert(right < it->left);
            right = max(right, it->right);
        }
        files.insert(files.end(), cur_file.begin(), cur_file.end());
        cur_file.clear();
    }

    static chunk *from_str(const char *p) {
        chunk *out = reinterpret_cast<chunk*>
            ((uintptr_t(p) - 1) & ~(kChunkSize - 1));
        assert(out->magic == CHUNK_MAGIC);
        return out;
    }
};

int chunk::chunk_files = 0;

const size_t kChunkSpace = kChunkSize - sizeof(chunk);

chunk *alloc_chunk() {
    void *p;
    if (posix_memalign(&p, kChunkSize, kChunkSize) != 0)
        return NULL;
    return new(p) chunk;
};

class chunk_allocator {
public:
    chunk_allocator() : current_(0) {
        new_chunk();
    }

    char *alloc(size_t len) {
        assert(len < kChunkSpace);
        if ((current_->size + len) > kChunkSpace)
            new_chunk();
        char *out = current_->data + current_->size;
        current_->size += len;
        return out;
    }

    list<chunk*>::iterator begin () {
        return chunks_.begin();
    }

    typename list<chunk*>::iterator end () {
        return chunks_.end();
    }

    chunk *current_chunk() {
        return current_;
    }

protected:
    void new_chunk() {
        current_ = alloc_chunk();
        chunks_.push_back(current_);
    }

    list<chunk*> chunks_;
    chunk *current_;
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
        matches_(0)
#ifdef PROFILE_CODESEARCH
        , re2_time_(false), our_time_(false)
#endif
    {
    }

    ~searcher() {
        log_profile("re2 time: %d.%06ds\n",
                    int(re2_time_.elapsed().tv_sec),
                    int(re2_time_.elapsed().tv_usec));
        log_profile("our time: %d.%06ds\n",
                    int(our_time_.elapsed().tv_sec),
                    int(our_time_.elapsed().tv_usec));
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

    bool operator()(const thread_state& ts, const chunk *chunk) {
        if (chunk == NULL) {
            queue_.push(NULL);
            return true;
        }
        StringPiece str(chunk->data, chunk->size);
        StringPiece match;
        int pos = 0, new_pos;
        timer re2_time(false), our_time(false);
        while (pos < str.size() && matches_.load() < kMaxMatches) {
            {
                run_timer run(re2_time);
                if (!pat_.Match(str, pos, str.size() - 1, RE2::UNANCHORED, &match, 1))
                    break;
            }
            {
                run_timer run(our_time);
                assert(memchr(match.data(), '\n', match.size()) == NULL);
                StringPiece line = find_line(str, match);
                if (utf8::is_valid(line.data(), line.data() + line.size()))
                    find_match(chunk, match, line, ts);
                new_pos = line.size() + line.data() - str.data() + 1;
                assert(new_pos > pos);
                pos = new_pos;
            }
        }
#ifdef PROFILE_CODESEARCH
        {
            mutex_locker locked(timer_mtx_);
            re2_time_.add(re2_time);
            our_time_.add(our_time);
        }
#endif
        if (matches_.load() >= kMaxMatches) {
            queue_.push(NULL);
            return true;
        }
        return false;
    }

protected:
    void find_match (const chunk *chunk,
                     const StringPiece& match,
                     const StringPiece& line,
                     const thread_state& ts) {
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
        struct timeval elapsed = tm.elapsed();
        log_profile("Searched %d files in %d.%06ds\n",
                    searched, int(elapsed.tv_sec), int(elapsed.tv_usec));
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
#ifdef PROFILE_CODESEARCH
    timer re2_time_;
    timer our_time_;
    mutex timer_mtx_;
#endif
};

code_searcher::code_searcher(git_repository *repo)
    : repo_(repo), stats_()
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

int code_searcher::match(RE2& pat) {
    list<chunk*>::iterator it;
    match_result *m;
    int matches = 0;
    int threads = 4;

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
    return matches;
}

void code_searcher::print_match(const match_result *m) {
    printf("%s:%s:%d:%d-%d: %.*s\n",
           m->file->ref,
           m->file->path.c_str(),
           m->lno,
           m->matchleft, m->matchright,
           m->line.size(), m->line.data());
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
