#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <google/dense_hash_set>

#include <locale>
#include <list>
#include <iostream>
#include <string>

#include <re2/re2.h>

#include "smart_git.h"
#include "timer.h"

using google::dense_hash_set;
using re2::RE2;
using re2::StringPiece;
using namespace std;

#define CHUNK_SIZE (1 << 20)

class chunk_allocator {
public:
    chunk_allocator() : current_() {
        new_chunk();
    }

    char *alloc(size_t len) {
        assert(len < CHUNK_SIZE);
        if (alloc_ + len > end_)
            new_chunk();
        char *out = alloc_;
        alloc_ += len;
        return out;
    }

    list<StringPiece>::iterator begin () {
        return chunks_.begin();
    }

    list<StringPiece>::iterator end () {
        return chunks_.end();
    }

    void finish(void) {
        new_chunk();
    }

protected:
    void new_chunk() {
        if (current_ != 0)
            chunks_.push_back(StringPiece(current_, alloc_ - current_));
        current_ = new char[CHUNK_SIZE];
        alloc_ = current_;
        end_ = current_ + CHUNK_SIZE;
    }

    list<StringPiece> chunks_;
    char *current_;
    char *alloc_;
    char *end_;
};


/*
 * We special-case data() == NULL to provide an "empty" element for
 * dense_hash_set.
 *
 * StringPiece::operator== will consider a zero-length string equal to a
 * zero-length string with a NULL data().
 */
struct eqstr {
    bool operator()(const StringPiece& lhs, const StringPiece& rhs) const {
        if (lhs.data() == NULL && rhs.data() == NULL)
            return true;
        if (lhs.data() == NULL || rhs.data() == NULL)
            return false;
        return lhs == rhs;
    }
};

struct hashstr {
    locale loc;
    size_t operator()(const StringPiece &str) const {
        const collate<char> &coll = use_facet<collate<char> >(loc);
        return coll.hash(str.data(), str.data() + str.size());
    }
};

const StringPiece empty_string(NULL, 0);

typedef dense_hash_set<StringPiece, hashstr, eqstr> string_hash;

class code_counter {
public:
    code_counter(git_repository *repo)
        : repo_(repo), stats_()
    {
        lines_.set_empty_key(empty_string);
    }

    void walk_ref(const char *ref) {
        smart_object<git_commit> commit;
        smart_object<git_tree> tree;
        resolve_ref(commit, ref);
        git_commit_tree(tree, commit);

        walk_tree(tree);
    }

    void dump_stats() {
        printf("Bytes: %ld (dedup: %ld)\n", stats_.bytes, stats_.dedup_bytes);
        printf("Lines: %ld (dedup: %ld)\n", stats_.lines, stats_.dedup_lines);
    }

    bool match(RE2& pat) {
        list<StringPiece>::iterator it;
        StringPiece match;
        int matches = 0;
        alloc_.finish();

        for (it = alloc_.begin(); it != alloc_.end(); it++) {
            int pos = 0;
            while (pos < (*it).size()) {
                    if (!pat.Match(*it, pos, (*it).size(), RE2::UNANCHORED, &match, 1))
                        break;
                    assert(memchr(match.data(), '\n', match.size()) == NULL);
                    StringPiece line = find_line(*it, match);
                    printf("%.*s\n", line.size(), line.data());
                    pos = line.size() + line.data() - (*it).data();
                    if (++matches == 10)
                        return true;
                }
        }
        return matches > 0;
    }
protected:
    StringPiece find_line(const StringPiece& chunk, const StringPiece& match) {
        const char *start, *end;
        assert(match.data() >= chunk.data());
        assert(match.data() < chunk.data() + chunk.size());
        assert(match.size() < (chunk.size() - (match.data() - chunk.data())));
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

    void walk_tree(git_tree *tree) {
        int entries = git_tree_entrycount(tree);
        int i;
        for (i = 0; i < entries; i++) {
            const git_tree_entry *ent = git_tree_entry_byindex(tree, i);
            smart_object<git_object> obj;
            git_tree_entry_2object(obj, repo_, ent);
            if (git_tree_entry_type(ent) == GIT_OBJ_TREE) {
                walk_tree(obj);
            } else if (git_tree_entry_type(ent) == GIT_OBJ_BLOB) {
                update_stats(obj);
            }
        }
    }

    void update_stats(git_blob *blob) {
        size_t len = git_blob_rawsize(blob);
        const char *p = static_cast<const char*>(git_blob_rawcontent(blob));
        const char *end = p + len;
        const char *f;
        string_hash::iterator it;

        while ((f = static_cast<const char*>(memchr(p, '\n', end - p))) != 0) {
            it = lines_.find(StringPiece(p, f - p));
            if (it == lines_.end()) {
                stats_.dedup_bytes += (f - p) + 1;
                stats_.dedup_lines ++;

                // Include the trailing '\n' in the chunk buffer
                char *alloc = alloc_.alloc(f - p + 1);
                memcpy(alloc, p, f - p + 1);
                lines_.insert(StringPiece(alloc, f - p));
            }
            p = f + 1;
            stats_.lines++;
        }

        stats_.bytes += len;
    }

    void resolve_ref(smart_object<git_commit> &out, const char *refname) {
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

    git_repository *repo_;
    string_hash lines_;
    struct {
        unsigned long bytes, dedup_bytes;
        unsigned long lines, dedup_lines;
    } stats_;
    chunk_allocator alloc_;
};

int main(int argc, char **argv) {
    git_repository *repo;
    git_repository_open(&repo, ".git");

    code_counter counter(repo);

    for (int i = 1; i < argc; i++) {
        timer tm;
        struct timeval elapsed;
        printf("Walking %s...", argv[i]);
        fflush(stdout);
        counter.walk_ref(argv[i]);
        elapsed = tm.elapsed();
        printf(" done in %d.%06ds\n",
               (int)elapsed.tv_sec, (int)elapsed.tv_usec);
    }
    counter.dump_stats();
    RE2::Options opts;
    opts.set_never_nl(true);
    opts.set_one_line(false);
    opts.set_posix_syntax(true);
    while (true) {
        printf("regex> ");
        string line;
        getline(cin, line);
        if (cin.eof())
            break;
        RE2 re(line, opts);
        if (re.ok()) {
            timer tm;
            struct timeval elapsed;
            if (!counter.match(re)) {
                printf("no match\n");
            }
            elapsed = tm.elapsed();
            printf("Match completed in %d.%06ds.\n",
                   (int)elapsed.tv_sec, (int)elapsed.tv_usec);
        }
    }

    return 0;
}
