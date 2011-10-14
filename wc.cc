#include <stdio.h>
#include <assert.h>

#include <git2.h>

class smart_object {
public:
    smart_object() : obj_(0) {
    };

    operator git_object** () {
        return &obj_;
    }

    operator git_tree* () {
        assert(git_object_type(obj_) == GIT_OBJ_TREE);
        return reinterpret_cast<git_tree*>(obj_);
    }

protected:
    git_object *obj_;
};

void lookup_head(git_repository *repo, const git_oid **oid) {
    git_reference *ref;
    git_reference_lookup(&ref, repo, "HEAD");
    if (git_reference_type(ref) == GIT_REF_SYMBOLIC) {
        const char *target = git_reference_target(ref);
        git_reference_lookup(&ref, repo, target);
    }
    *oid = git_reference_oid(ref);
}

void walk_tree(git_repository *repo, git_tree *tree, int indent) {
    int entries = git_tree_entrycount(tree);
    int i;
    for (i = 0; i < entries; i++) {
        const git_tree_entry *ent = git_tree_entry_byindex(tree, i);
        printf("%*s%s\n", indent, " ", git_tree_entry_name(ent));
        if (git_tree_entry_type(ent) == GIT_OBJ_TREE) {
            smart_object subtree;
            git_tree_entry_2object(subtree, repo, ent);
            walk_tree(repo, subtree, indent + 1);
        }
    }
}

int main(int argc, char **argv) {
    git_repository *repo;
    git_repository_open(&repo, ".git");

    const git_oid *oid;
    lookup_head(repo, &oid);

    git_commit *commit;
    git_tree *tree;
    git_commit_lookup(&commit, repo, oid);
    git_commit_tree(&tree, commit);

    walk_tree(repo, tree, 0);
    git_tree_close(tree);

    return 0;
}
