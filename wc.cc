#include <stdio.h>
#include <assert.h>

#include <git2.h>

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

class code_counter {
public:
    code_counter(git_repository *repo) : repo_(repo) {
    }

    void run() {
        const git_oid *oid;
        lookup_head(&oid);

        smart_object<git_commit> commit;
        smart_object<git_tree> tree;
        git_commit_lookup(commit, repo_, oid);
        git_commit_tree(tree, commit);

        walk_tree(tree);

        printf("Bytes: %ld\n", bytes);
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
        bytes += git_blob_rawsize(blob);
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
    unsigned long bytes;
};

int main(int argc, char **argv) {
    git_repository *repo;
    git_repository_open(&repo, ".git");

    code_counter counter(repo);
    counter.run();

    return 0;
}
