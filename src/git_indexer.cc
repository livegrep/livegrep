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
                         const google::protobuf::RepeatedPtrField<RepoSpec>& repositories)
    : cs_(cs), repo_(0), repositories_to_index_(repositories) {
    int err;
    if ((err = git_libgit2_init()) < 0)
        die("git_libgit2_init: %s", giterr_last()->message);

    /* git_repository_open(&repo_, repopath.c_str()); */
    /* if (repo_ == NULL) { */
    /*     fprintf(stderr, "Unable to open repo: %s\n", repopath.c_str()); */
    /*     exit(1); */
    /* } */
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_BLOB, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_OFS_DELTA, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_REF_DELTA, 10*1024);
}

git_indexer::~git_indexer() {
    // This should do nothing if repo_ is null
    git_repository_free(repo_);
}

void git_indexer::begin_indexing() {
    // first, go through all repos and sort all the files possible
    // then, go through and actually call cs.index_file on all of them

    // The below will populate files_to_index_
    for (auto &repo : repositories_to_index_) {
        repopath_ = repo.path();
        const char *repopath = repopath_.c_str();

        fprintf(stderr, "indexing repo: %s\n", repopath);
        // if repo has already been set AND it's not the same as this one
        if (repo_ != NULL && strcmp(git_repository_path(repo_), repopath) != 0 ) {
            // Free the old repository
            git_repository_free(repo_);

            // open the new repository
            int err = git_repository_open(&repo_, repopath);
            if (err < 0) {
                fprintf(stderr, "Unable to open repo: %s\n", repopath);
                exit(1);
            }
        } else { // if we haven't opened a repo yet
            int err = git_repository_open(&repo_, repopath);
            if (err < 0) {
                fprintf(stderr, "Unable to open repo: %s\n", repopath);
                exit(1);
            }
        }

        name_ = repo.name();
        metadata_ = repo.metadata();
        walk_submodules_ = repo.walk_submodules();

        for (auto &rev : repo.revisions()) {
            walk(rev);
        }
    }

    // close the most recent repo opened
    git_repository_free(repo_);
    index_files();
}


void git_indexer::index_files() {
    // the idea here would be to get all the files from all repos.
    // Then sort them
    // How would we go back to every file?
    fprintf(stderr, "have %ld files\n", files_to_index_.size());
    // we seem to have an extra file?
    // pre-sort
    for (auto it = files_to_index_.begin(); it != files_to_index_.end(); ++it) {
        auto file = it->get();

        const char *repopath = file->repopath.c_str();
        fprintf(stderr, "%s/%s. id: %s \n", repopath, file->path.c_str(), file->id.c_str()); 

        git_oid blob_id;
        int err = git_oid_fromstr(&blob_id, file->id.c_str());

        if (err < 0) {
            const git_error *e = giterr_last();
            printf("Error %d/%d: %s\n", err, e->klass, e->message);
            exit(err);
        }

        const git_oid blob_id_static = static_cast<git_oid>(blob_id);

        /* git_repository_open(&repo_, repopath); */

        fprintf(stderr, "open at: %s\n", git_repository_path(repo_));

        
        if (git_repository_path(repo_) != NULL && strcmp(git_repository_path(repo_), repopath) != 0 ) {
            fprintf(stderr, "freeing the old repo...\n");
            // Free the old repository
            git_repository_free(repo_);

            // open the new repository
            git_repository_open(&repo_, repopath);
            if (repo_ == NULL) {
                fprintf(stderr, "Unable to open repo: %s\n", repopath);
                exit(1);
            }
        } else if (git_repository_path(repo_) == NULL) { // if we haven't opened a repo yet
            fprintf(stderr, "repo_ is null, trying to open..\n");
            git_repository_open(&repo_, repopath);
            if (repo_ == NULL) {
                fprintf(stderr, "Unable to open repo: %s\n", repopath);
                exit(1);
            }
        }

        fprintf(stderr, "_repo opened at path: %s\n", git_repository_path(repo_));

        // now that the repo is open
        git_blob *blob;
        /* int err = git_blob_lookup(&blob, repo_, file->id); */

        err = git_blob_lookup(&blob, repo_, &blob_id_static);

        if (err < 0) {
            const git_error *e = giterr_last();
            printf("Error %d/%d: %s\n", err, e->klass, e->message);
            exit(err);
        }


        fprintf(stderr, "blob looked up, (theoretically). Owner: %s\n", git_repository_path(git_blob_owner(blob)));

        const char *data = static_cast<const char*>(git_blob_rawcontent(blob));

        fprintf(stderr, "blob: %s\n", data);

        fprintf(stderr, "data loaded (theoretically)\n");
        cs_->index_file(file->tree, file->path, StringPiece(data, git_blob_rawsize(blob)));

    }

    fprintf(stderr, "finished looping\n");
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
            const git_oid* blob_id = git_blob_id(obj);
            char blob_id_str[GIT_OID_HEXSZ + 1];
            git_oid_tostr(blob_id_str, GIT_OID_HEXSZ + 1, blob_id);

            const string full_path = submodule_prefix_ + path;
            auto file = std::make_unique<pre_indexed_file>();
            file->tree = idx_tree_;
            file->repopath = repopath_;
            file->path =  full_path;

            // Hmm, so I think this is a git_tree_entry, so that's why the the
            // id doesn't correspond to a blob per-se
            file->id = string(blob_id_str);
            file->id_test2 = (blob_id)->id;

            // I need to copy the oid back and forth, otherwise I run into that
            // indeterminate behavior thats described
            
            fprintf(stderr, "%s/%s -> %s\n", repopath_.c_str(), full_path.c_str(), git_oid_tostr_s(blob_id));
            fprintf(stderr, "id_test: %s\n", file->id.c_str());
            fprintf(stderr, "id_test2 raw 20 bytes: [%s]\n", file->id_test2);

            files_to_index_.push_back(std::move(file));


            /* const char *data = static_cast<const char*>(git_blob_rawcontent(obj)); */
            /* cs_->index_file(idx_tree_, submodule_prefix_ + path, StringPiece(data, git_blob_rawsize(obj))); */
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

            /* git_indexer sub_indexer(cs_, sub_repopath, string(sub_name), meta, walk_submodules_); */
            /* sub_indexer.submodule_prefix_ = submodule_prefix_ + path + "/"; */

            /* const git_oid* rev = git_tree_entry_id(*it); */
            /* char revstr[GIT_OID_HEXSZ + 1]; */
            /* git_oid_tostr(revstr, GIT_OID_HEXSZ + 1, rev); */

            /* sub_indexer.walk(string(revstr)); */
        }
    }
}
