#include <gflags/gflags.h>
#include <sstream>

#include "src/lib/metrics.h"
#include "src/lib/debug.h"

#include "src/codesearch.h"
#include "src/git_indexer.h"
#include "src/smart_git.h"
#include "src/score.h"

#include "src/proto/config.pb.h"

using namespace std;

DEFINE_string(order_root, "", "Walk top-level directories in this order.");
DEFINE_bool(revparse, false, "Display parsed revisions, rather than as-provided");

git_indexer::git_indexer(code_searcher *cs,
                         const google::protobuf::RepeatedPtrField<RepoSpec>& repositories)
    : cs_(cs), repositories_to_index_(repositories) {
    int err;
    if ((err = git_libgit2_init()) < 0)
        die("git_libgit2_init: %s", giterr_last()->message);

    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_BLOB, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_OFS_DELTA, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_REF_DELTA, 10*1024);
}

git_indexer::~git_indexer() {
    git_libgit2_shutdown();

    /* for (auto it = threads_.begin(); it != threads_.end(); ++it) */
    /*     it->join(); */
}

void git_indexer::print_last_git_err_and_exit(int err) {
    const git_error *e = giterr_last();
    printf("Error %d/%d: %s\n", err, e->klass, e->message);
    exit(1);
}

// will be called by a thread to walk a subsection of repositories_to_index_
void git_indexer::walk_repositories_subset(int start, int end) {
    // ideally, walk will post to a thread_local files_to_index_.

    fprintf(stderr, "walk_repositories_subset: %d-%d\n", start, end);
    /* return; */
    // then each thread pushes to files_to_index_local?? Cool, if it's that
    // simple.
    for (int i = start; i < end; ++i) {
        const auto &repo = repositories_to_index_[i];
        const char *repopath = repo.path().c_str();

        /* fprintf(stderr, "walking repo: %s\n", repopath); */
        git_repository *curr_repo = NULL;

        int err = git_repository_open(&curr_repo, repopath);
        if (err < 0) {
            print_last_git_err_and_exit(err);
        }

        for (auto &rev : repo.revisions()) {
            fprintf(stderr, "walking %s at %s \n", repopath, rev.c_str());
            walk(curr_repo, rev, repo.path(), repo.name(), repo.metadata(), repo.walk_submodules(), "");
            fprintf(stderr, "done walking %s at %s\n", repopath, rev.c_str());
        }

        git_repository_free(curr_repo);
    }

    // Then at the end we'll post to files_to_index_
    // Then we're done
    fprintf(stderr, "trying to do our wild mutex backed append\n");
    std::lock_guard<std::mutex> guard(files_mutex_);
    files_to_index_.insert(files_to_index_.end(), std::make_move_iterator(files_to_index_local.get()->begin()), std::make_move_iterator(files_to_index_local.get()->end()));
}

void git_indexer::begin_indexing() {

    // min_per_thread will require tweaking. For example, even with only
    // 2 repos, would it not be worth it to spin up two threads (if available)?
    // Or would the overhead of the thread creation far outweigh the single-core
    // performance for just a few repos.
    
    unsigned long const length = repositories_to_index_.size();
    unsigned long const min_per_thread = 5; 
    unsigned long const max_threads = 
        (length + min_per_thread - 1)/min_per_thread;
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    unsigned long const num_threads = std::min(hardware_threads!=0?hardware_threads:2, max_threads);
    // We round block size up. the last thread may have less items to work with
    // when length is uneven
    unsigned long const block_size = (length + num_threads - 1) / (num_threads);

    fprintf(stderr, "length=%lu min_per_thread=%lu max_threads=%lu hardware_threads=%lu num_threads=%lu block_size=%lu\n", length, min_per_thread, max_threads, hardware_threads, num_threads, block_size);

    if (length < 2 * min_per_thread) {
        fprintf(stderr, "Not going to create any new threads.\n");
        walk_repositories_subset(0, length);
        index_files();
        return;
    }

    threads_.reserve(num_threads - 1);
    long block_start = 0;
    for (long i = 0; i < num_threads; ++i) {
        long block_end = std::min(block_start + block_size, length);
        threads_.emplace_back(&git_indexer::walk_repositories_subset, this, (int)block_start, (int)block_end);
        block_start = block_end;
    }

    for (long i = 0; i < num_threads; ++i) {
        threads_[i].join();
    }

    /* exit(1); */
    // Then we'd spin up num_threads into a vector.
    // Then each thread can chew through `block_size + offset` repos.
    //
    // Right now, we push every single file onto files_to_index_. 
    // To avoid contention, we could have each thread keep
    // a thread_files_to_index, and then when done use vector::insert
    // to append to files_to_index_. How do we access thread local storage?

    // populate files_to_index_
    /* for (auto &repo : repositories_to_index_) { */
    /*     const char *repopath = repo.path().c_str(); */

    /*     fprintf(stderr, "walking repo: %s\n", repopath); */
    /*     git_repository *curr_repo = NULL; */

    /*     int err = git_repository_open(&curr_repo, repopath); */
    /*     if (err < 0) { */
    /*         print_last_git_err_and_exit(err); */
    /*     } */

    /*     for (auto &rev : repo.revisions()) { */
    /*         fprintf(stderr, " walking %s... ", rev.c_str()); */
    /*         walk(curr_repo, rev, repo.path(), repo.name(), repo.metadata(), repo.walk_submodules(), ""); */
    /*         fprintf(stderr, "done\n"); */
    /*     } */

    /*     git_repository_free(curr_repo); */
    /* } */

    index_files();
}

bool operator<(std::unique_ptr<pre_indexed_file>& a, std::unique_ptr<pre_indexed_file>& b) {
    return a->score > b->score;
}

void git_indexer::index_files() {
    fprintf(stderr, "sorting %lu files_to_index_... ", files_to_index_.size());
    std::stable_sort(files_to_index_.begin(), files_to_index_.end());
    fprintf(stderr, "done sorting files\n");

    git_repository *curr_repo = NULL;
    const char *prev_repopath = "";

    fprintf(stderr, "walking files_to_index_ ...\n");
    for (auto it = files_to_index_.begin(); it != files_to_index_.end(); ++it) {
        auto file = it->get();

        const char *repopath = file->repopath.c_str();

        /* fprintf(stderr, "indexing %s/%s\n", repopath, file->path.c_str()); */
        
        if (strcmp(prev_repopath, repopath) != 0) {
            git_repository_free(curr_repo); // Will do nothing if curr_repo == NULL
            int err = git_repository_open(&curr_repo, repopath);
            if (err < 0) {
                print_last_git_err_and_exit(err);
            }
        }

        git_oid blob_id;
        int err = git_oid_fromstr(&blob_id, file->id.c_str());

        if (err < 0) {
            print_last_git_err_and_exit(err);
        }

        const git_oid blob_id_static = static_cast<git_oid>(blob_id);
        git_blob *blob;
        err = git_blob_lookup(&blob, curr_repo, &blob_id_static);

        if (err < 0) {
            print_last_git_err_and_exit(err);
        }

        const char *data = static_cast<const char*>(git_blob_rawcontent(blob));
        cs_->index_file(file->tree, file->path, StringPiece(data, git_blob_rawsize(blob)));

        git_blob_free(blob);
        prev_repopath = repopath;
    }

    fprintf(stderr, "finished with index_files\n");
}

void git_indexer::walk(git_repository *curr_repo,
        const string& ref, 
        const string repopath, 
        const string& name,
        Metadata metadata,
        bool walk_submodules,
        const string& submodule_prefix) {
    smart_object<git_commit> commit;
    smart_object<git_tree> tree;
    if (0 != git_revparse_single(commit, curr_repo, (ref + "^0").c_str())) {
        fprintf(stderr, "ref %s not found, skipping (empty repo?)\n", ref.c_str());
        return;
    }
    git_commit_tree(tree, commit);
    /* fprintf(stderr, "opened commit_tree for %s\n", repopath.c_str()); */

    char oidstr[GIT_OID_HEXSZ+1];
    string version = FLAGS_revparse ?
        strdup(git_oid_tostr(oidstr, sizeof(oidstr), git_commit_id(commit))) : ref;

    const indexed_tree *idx_tree = cs_->open_tree(name, metadata, version);
    walk_tree("", FLAGS_order_root, repopath, walk_submodules, submodule_prefix, idx_tree, tree, curr_repo);
}


void git_indexer::walk_tree(const string& pfx,
                            const string& order,
                            const string repopath,
                            bool walk_submodules,
                            const string& submodule_prefix,
                            const indexed_tree *idx_tree,
                            git_tree *tree,
                            git_repository *curr_repo) {
    /* fprintf(stderr, "preparing to walk_tree for %s with prefix: %s \n", repopath.c_str(), pfx.c_str()); */
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
        git_tree_entry_to_object(obj, curr_repo, *it);
        string path = pfx + git_tree_entry_name(*it);

        /* fprintf(stderr, "walking obj with path: %s/%s\n", repopath.c_str(), path.c_str()); */

        if (git_tree_entry_type(*it) == GIT_OBJ_TREE) {
            walk_tree(path + "/", "", repopath, walk_submodules, submodule_prefix, idx_tree, obj, curr_repo);
        } else if (git_tree_entry_type(*it) == GIT_OBJ_BLOB) {
            const git_oid* blob_id = git_blob_id(obj);
            char blob_id_str[GIT_OID_HEXSZ + 1];
            git_oid_tostr(blob_id_str, GIT_OID_HEXSZ + 1, blob_id);

            const string full_path = submodule_prefix + path;
            auto file = std::make_unique<pre_indexed_file>();
            file->id = string(blob_id_str);
            file->tree = idx_tree;
            file->repopath = repopath;
            file->path =  full_path;
            file->score = score_file(full_path);

            // Ok, so this isn't working either
            /* fprintf(stderr, "indexing %s/%s\n", repopath.c_str(), file->path.c_str()); */
            if (!files_to_index_local.get()) {
                files_to_index_local.put(new vector<unique_ptr<pre_indexed_file>>());
            }
            files_to_index_local.get()->push_back(std::move(file));

        } else if (git_tree_entry_type(*it) == GIT_OBJ_COMMIT) {
            // Submodule
            if (!walk_submodules) {
                continue;
            }
            git_submodule* submod = nullptr;
            if (0 != git_submodule_lookup(&submod, curr_repo, path.c_str())) {
                fprintf(stderr, "Unable to get submodule entry for %s, skipping\n", path.c_str());
                continue;
            }

            const char* sub_name = git_submodule_name(submod);
            string sub_repopath = repopath + "/" + path;
            string new_submodule_prefix = submodule_prefix + path + "/";
            Metadata meta;

            const git_oid* rev = git_tree_entry_id(*it);
            char revstr[GIT_OID_HEXSZ + 1];
            git_oid_tostr(revstr, GIT_OID_HEXSZ + 1, rev);

            // Open the submodule repo
            git_repository *sub_repo;

            int err = git_repository_open(&sub_repo, sub_repopath.c_str());

            if (err < 0) {
                fprintf(stderr, "Unable to open subrepo: %s\n", sub_repopath.c_str());
                print_last_git_err_and_exit(err);
            }

            walk(sub_repo, string(revstr), sub_repopath, string(sub_name), meta, walk_submodules, new_submodule_prefix);

            git_repository_free(sub_repo);
        }
    }
}
