#include <stdio.h>

extern "C" {
#include <git2.h>
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

void walk_tree(git_tree *tree) {
    
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

    walk_tree(tree);

    return 0;
}
