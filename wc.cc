#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <google/dense_hash_set>
#include <unordered_set>

#include <git2.h>

using google::dense_hash_set;
using std::hash;

struct eqstr {
    bool operator()(const char* s1, const char* s2) const
    {
        return (s1 == s2) || (s1 && s2 && strcmp(s1, s2) == 0);
    }
};

class smart_object_base {
public:
    smart_object_base() : obj_(0) {
    };

    operator git_object** () {
        return &obj_;
    }

    ~smart_object_base() {
        if (obj_)
            git_object_close(obj_);
    }

protected:
    smart_object_base(const smart_object_base& rhs) {
    }
    git_object *obj_;
};

template <class T>
class object_traits { const static git_otype type; };

template <>
struct object_traits<git_tree> { const static git_otype git_type = GIT_OBJ_TREE; };
template <>
struct object_traits<git_commit> { const static git_otype git_type = GIT_OBJ_COMMIT; };
template <>
struct object_traits<git_blob> { const static git_otype git_type = GIT_OBJ_BLOB; };

template <class T>
class smart_object : public smart_object_base {
public:
    operator T* () {
        assert(obj_);
        assert(git_object_type(obj_) == object_traits<T>::git_type);
        return reinterpret_cast<T*>(obj_);
    }
    operator T** () {
        assert(obj_ == 0);
        return reinterpret_cast<T**>(&obj_);
    }
};

template <>
class smart_object <git_object> : public smart_object_base {
public:
    template <class O>
    operator O* () {
        assert(git_object_type(obj_) == object_traits<O>::git_type);
        return reinterpret_cast<O*>(obj_);
    }
};

typedef dense_hash_set<const char*, hash<const char*>, eqstr> string_hash;

class code_counter {
public:
    code_counter(git_repository *repo)
        : repo_(repo), bytes_(0), dedup_bytes_(0),
          line_count_(0), dedup_line_count_(0)
    {
        lines_.set_empty_key(NULL);
    }

    void run() {
        const git_oid *oid;
        lookup_head(&oid);

        smart_object<git_commit> commit;
        smart_object<git_tree> tree;
        git_commit_lookup(commit, repo_, oid);
        git_commit_tree(tree, commit);

        walk_tree(tree);

        printf("Bytes: %ld (dedup: %ld)\n", bytes_, dedup_bytes_);
        printf("Lines: %ld (dedup: %ld)\n", line_count_, dedup_line_count_);
    }
protected:
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
        char *str;
        size_t len = git_blob_rawsize(blob);
        char *p = new char[len];
        char *end = p + len;
        char *f;
        string_hash::iterator it;
        memcpy(p, git_blob_rawcontent(blob), len);

        while ((f = static_cast<char*>(memchr(p, '\n', end - p))) != 0) {
            *f = '\0';
            it = lines_.find(p);
            if (it == lines_.end()) {
                lines_.insert(p);
                dedup_bytes_ += (f - p);
                dedup_line_count_++;
            }
            p = f + 1;
            line_count_++;
        }

        bytes_ += len;
    }


    void lookup_head(const git_oid **oid) {
        git_reference *ref;
        git_reference_lookup(&ref, repo_, "HEAD");
        if (git_reference_type(ref) == GIT_REF_SYMBOLIC) {
            const char *target = git_reference_target(ref);
            git_reference_lookup(&ref, repo_, target);
        }
        *oid = git_reference_oid(ref);
    }

    git_repository *repo_;
    unsigned long bytes_, dedup_bytes_;
    unsigned long line_count_, dedup_line_count_;
    string_hash lines_;
};

int main(int argc, char **argv) {
    git_repository *repo;
    git_repository_open(&repo, ".git");

    code_counter counter(repo);
    counter.run();

    return 0;
}
