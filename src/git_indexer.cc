#include <gflags/gflags.h>
#include <sstream>

#include "src/lib/metrics.h"
#include "src/lib/debug.h"

#include "src/codesearch.h"
#include "src/git_indexer.h"
#include "src/smart_git.h"

#include "src/proto/config.pb.h"

using namespace std;

DEFINE_string(order_root, "", "Walk top-level directories in this order.");
DEFINE_bool(revparse, false, "Display parsed revisions, rather than as-provided");

git_indexer::git_indexer(code_searcher *cs,
                         const string& repopath,
                         const string& name,
                         const Metadata &metadata,
                         bool walk_submodules)
    : cs_(cs), repo_(0), repopath_(repopath), name_(name), metadata_(metadata)
    , walk_submodules_(walk_submodules) {
    int err;
    if ((err = git_libgit2_init()) < 0)
        die("git_libgit2_init: %s", giterr_last()->message);

    git_repository_open(&repo_, repopath.c_str());
    if (repo_ == NULL) {
        fprintf(stderr, "Unable to open repo: %s\n", repopath.c_str());
        exit(1);
    }
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_BLOB, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_OFS_DELTA, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_REF_DELTA, 10*1024);
}

git_indexer::~git_indexer() {
    git_repository_free(repo_);
}

void git_indexer::walk(const string& ref) {
    smart_object<git_commit> commit;
    smart_object<git_tree> tree;
    if (0 != git_revparse_single(commit, repo_, (ref + "^0").c_str())) {
        fprintf(stderr, "ref %s not found, skipping (empty repo?)\n", ref.c_str());
        return;
    }
    git_commit_tree(tree, commit);

    char oidstr[GIT_OID_HEXSZ+1];
    string version = FLAGS_revparse ?
        strdup(git_oid_tostr(oidstr, sizeof(oidstr), git_commit_id(commit))) : ref;

    idx_tree_ = cs_->open_tree(name_, metadata_, version);
    walk_tree("", FLAGS_order_root, tree);
}

void git_indexer::walk_tree(const string& pfx,
                            const string& order,
                            git_tree *tree) {
    map<string, const git_tree_entry *> root;
    vector<const git_tree_entry *> ordered;
    int entries = git_tree_entrycount(tree);
    for (int i = 0; i < entries; ++i) {
        const git_tree_entry *ent = git_tree_entry_byindex(tree, i);
        root[git_tree_entry_name(ent)] = ent;
    }

    istringstream stream(order);
    string dir;
    while(stream >> dir) {
        map<string, const git_tree_entry *>::iterator it = root.find(dir);
        if (it == root.end())
            continue;
        ordered.push_back(it->second);
        root.erase(it);
    }
    for (map<string, const git_tree_entry *>::iterator it = root.begin();
         it != root.end(); ++it)
        ordered.push_back(it->second);
    for (vector<const git_tree_entry *>::iterator it = ordered.begin();
         it != ordered.end(); ++it) {
        smart_object<git_object> obj;
        git_tree_entry_to_object(obj, repo_, *it);
        string path = pfx + git_tree_entry_name(*it);

        if (git_tree_entry_type(*it) == GIT_OBJ_TREE) {
            walk_tree(path + "/", "", obj);
        } else if (git_tree_entry_type(*it) == GIT_OBJ_BLOB) {
            const char *data = static_cast<const char*>(git_blob_rawcontent(obj));
            cs_->index_file(idx_tree_, submodule_prefix_ + path, StringPiece(data, git_blob_rawsize(obj)));
        } else if (git_tree_entry_type(*it) == GIT_OBJ_COMMIT) {
            // Submodule
            if (!walk_submodules_) {
                continue;
            }
            git_submodule* submod = nullptr;
            if (0 != git_submodule_lookup(&submod, repo_, path.c_str())) {
                fprintf(stderr, "Unable to get submodule entry for %s, skipping\n", path.c_str());
                continue;
            }
            const char* sub_name = git_submodule_name(submod);
            string sub_repopath = repopath_ + "/" + path;
            Metadata meta;

            git_indexer sub_indexer(cs_, sub_repopath, string(sub_name), meta, walk_submodules_);
            sub_indexer.submodule_prefix_ = submodule_prefix_ + path + "/";

            const git_oid* rev = git_tree_entry_id(*it);
            char revstr[GIT_OID_HEXSZ + 1];
            git_oid_tostr(revstr, GIT_OID_HEXSZ + 1, rev);

            sub_indexer.walk(string(revstr));
        }
    }
}
