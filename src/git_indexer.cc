#include <gflags/gflags.h>
#include <sstream>

#include "src/lib/metrics.h"
#include "src/lib/debug.h"
#include "src/lib/threadsafe_progress_indicator.h"
#include "src/lib/rlimits.h"

#include "src/codesearch.h"
#include "src/git_indexer.h"
#include "src/smart_git.h"
#include "src/score.h"

#include "src/proto/config.pb.h"

using namespace std;
using namespace std::chrono;

DEFINE_string(order_root, "", "Walk top-level directories in this order.");
DEFINE_bool(revparse, false, "Display parsed revisions, rather than as-provided");
DEFINE_bool(increase_fds, true, "Increase proc file descriptors from the soft limit to the hard limit");
DEFINE_bool(allow_multithreading, true, "Allow multithreaded git repo walking");

git_indexer::git_indexer(code_searcher *cs,
                         const google::protobuf::RepeatedPtrField<RepoSpec>& repositories)
    : cs_(cs), repositories_to_index_(repositories), repositories_to_index_length_(repositories.size()), trees_to_walk_() {
    int err;
    if ((err = git_libgit2_init()) < 0)
        die("git_libgit2_init: %s", giterr_last()->message);

    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_BLOB, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_OFS_DELTA, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_REF_DELTA, 10*1024);
}

git_indexer::~git_indexer() {
    for (auto it = open_git_repos_.begin(); it != open_git_repos_.end(); ++it) {
        git_repository_free(*(it));
    }
    git_libgit2_shutdown();
}



void git_indexer::print_last_git_err_and_exit(int err) {
    const git_error *e = giterr_last();
    printf("Error %d/%d: %s\n", err, e->klass, e->message);
    exit(1);
}

void git_indexer::process_trees() {
    tree_to_walk *d;

    while (trees_to_walk_.pop(&d)) {
        walk_tree(d->prefix, "", d->repopath, d->walk_submodules, 
                d->submodule_prefix, d->idx_tree, d->tree, d->repo, 1);
        git_tree_free(d->tree);
        delete(d);
    }
}

int git_indexer::get_num_threads_to_use() {
    unsigned long const min_per_thread = 16; 
    unsigned long const max_threads = (repositories_to_index_length_ + min_per_thread - 1) / min_per_thread;
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    unsigned long num_threads = std::min(hardware_threads, max_threads);

    // We enforce single-threaded behavior when someone has a specific order
    // they want to visit things in
    if (FLAGS_order_root != "") {
        fprintf(stderr, "order_root is set, walking repos in single-threaded mode...\n");
        num_threads = 0;
    } else if (!FLAGS_allow_multithreading) {
        fprintf(stderr, "Multithreading disabled, walking repos in single-threaded mode...\n");
        num_threads = 0;
    } else if (num_threads == 1) {
        // 1 thread is pointless. It takes more time to spin up than indexing
        // would take.
        num_threads = 0;
    }

    fprintf(stderr, "length=%d min_per_thread=%lu max_threads=%lu hardware_threads=%lu num_threads=%lu\n", 
            repositories_to_index_length_, min_per_thread, max_threads, hardware_threads, num_threads);

    return num_threads;
}

void git_indexer::index_repos() {
    int num_threads = get_num_threads_to_use();
    is_singlethreaded_ = num_threads == 0;


    // we assume the average number of files per repo is 1000. This is very not
    // true for most repos, but there are plenty of huge repos that have
    // multiple thousands of files that make up the difference.
    files_to_index_.reserve(repositories_to_index_length_ * 1000);
    open_git_repos_.reserve(repositories_to_index_length_);
    threads_.reserve(num_threads);
    for (long i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&git_indexer::process_trees, this);
    }

    // in both single and multi thread modes, try to bump the number of file
    // descriptors to the max allowed by the OS. We don't do any fancy attempts
    // at keeping the number of files <= max, we mostly just hope it will be
    // enough. We can definitely keep track of that if it becomes necessary.
    if (FLAGS_increase_fds) {
        increaseOpenFileLimitToMax();
    }

    int idx = 1;
    for (const auto &repo : repositories_to_index_) {
        const char *repopath = repo.path().c_str();

        git_repository *curr_repo = NULL;

        int err = git_repository_open(&curr_repo, repopath);
        if (err < 0) {
            print_last_git_err_and_exit(err);
        }
        open_git_repos_.push_back(curr_repo);

        fprintf(stderr, "Walking repo_spec [%d/%d] name=%s, path=%s (including submodules: %s)\n",
                idx, repositories_to_index_length_, repo.name().c_str(), repo.path().c_str(), 
                repo.walk_submodules() ? "true" : "false");
        for (auto &rev : repo.revisions()) {
            fprintf(stderr, " walking %s... ", rev.c_str());
            walk(curr_repo, rev, repo.path(), repo.name(), repo.metadata(), repo.walk_submodules(), "");
            fprintf(stderr, "done\n");
        }
        idx += 1;
    }

    // we can close the trees, since we only add to trees_to_walk_ at the
    // root level of walk_tree, so once we've walked all repos we won't be
    // adding anymore trees.
    trees_to_walk_.close();

    // this is a compromise between performance and wanting to give the user
    // some indication of progress. We poll at some interval that doesn't
    // intrude on the mutex too much, until there are no trees to work on.
    // Once size is 0, that doesn't mean there are no trees being worked on, 
    // so we still have to wait for the threads to join
    fprintf(stderr, "\nwalking repo trees...\n");
    for(;;)
    {
        int curr = trees_to_walk_.size();
        if (curr == 0) break;
        fprintf(stderr, "    %d remaining\n", curr);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    for (auto it = threads_.begin(); it != threads_.end(); ++it) {
        it->join();
    }
    fprintf(stderr, "    done.\n");
    
    index_files();
}

bool compareFilesByScore(pre_indexed_file *a, pre_indexed_file *b) {
    return a->score > b->score;
}

bool compareFilesByTree(pre_indexed_file *a, pre_indexed_file *b) {
    return a->tree->name < b->tree->name;
}

// sorts `files_to_index_` based on score. This way, the lowest scoring files
// get indexed last, and so show up in sorts results last. We use a stable sort
// so that most of a repos files are indexed together
void git_indexer::index_files() {

    // in multi-threaded mode, we also sort by tree since a tree/repos files could
    // have ended up at any index. If FLAGS_order_root is set, then
    // multi-threading is disabled, so we don't have to re-check
    if (!is_singlethreaded_) {
        fprintf(stderr, "sorting files_to_index_ by tree...\n");
        std::stable_sort(files_to_index_.begin(), files_to_index_.end(), compareFilesByTree);
        fprintf(stderr, "  done\n");
    }

    fprintf(stderr, "sorting files_to_index_ by score...\n");
    std::stable_sort(files_to_index_.begin(), files_to_index_.end(), compareFilesByScore);
    fprintf(stderr, "  done\n");

    threadsafe_progress_indicator tpi(files_to_index_.size(), "Indexing files_to_index_...", "Done");
    for (auto it = files_to_index_.begin(); it != files_to_index_.end(); ++it) {
        auto file = *it;

        git_blob *blob;
        int err = git_blob_lookup(&blob, file->repo, file->oid);
        free(file->oid);

        if (err < 0) {
            print_last_git_err_and_exit(err);
        }
        
        const char *data = static_cast<const char*>(git_blob_rawcontent(blob));
        cs_->index_file(file->tree, file->path, StringPiece(data, git_blob_rawsize(blob)));

        git_blob_free(blob);
        delete(file);
        tpi.tick();
    }
}

void git_indexer::walk(git_repository *curr_repo,
        const std::string& ref,
        const std::string& repopath,
        const std::string& name,
        Metadata metadata,
        bool walk_submodules,
        const std::string& submodule_prefix) {
    smart_object<git_commit> commit;
    smart_object<git_tree> tree;
    if (0 != git_revparse_single(commit, curr_repo, (ref + "^0").c_str())) {
        fprintf(stderr, "%s: ref %s not found, skipping (empty repo?)\n", name.c_str(), ref.c_str());
        return;
    }
    git_commit_tree(tree, commit);

    char oidstr[GIT_OID_HEXSZ+1];
    string version = FLAGS_revparse ?
        strdup(git_oid_tostr(oidstr, sizeof(oidstr), git_commit_id(commit))) : ref;

    const indexed_tree *idx_tree = cs_->open_tree(name, metadata, version);
    walk_tree("", FLAGS_order_root, repopath, walk_submodules, submodule_prefix, idx_tree, tree, curr_repo, 0);
}


void git_indexer::walk_tree(const std::string& pfx,
                            const std::string& order,
                            const std::string& repopath,
                            bool walk_submodules,
                            const std::string& submodule_prefix,
                            const indexed_tree *idx_tree,
                            git_tree *tree,
                            git_repository *curr_repo,
                            int depth) {
    map<string, const git_tree_entry *> root;
    vector<pair<const string, const git_tree_entry *>> ordered;
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
        ordered.push_back(make_pair(it->first, it->second));
        root.erase(it);
    }
    for (map<string, const git_tree_entry *>::iterator it = root.begin();
         it != root.end(); ++it)
        ordered.push_back(make_pair(it->first, it->second));

    for (vector<pair<const string, const git_tree_entry *>>::iterator it = ordered.begin();
        it != ordered.end(); ++it) {
        
        // We use a plain git_object here rather than a smart object, since
        // attaching a smart_object to tree_to_walk causes a segfault once we
        // try to access it, I'm assuming because the smart_object has been
        // released already. Not sure if I can avoid this somehow...
        // For now, use a plain git_object. If the object is a blob, we free it
        // right away. If it's a tree we free it either right away or later
        // depending on is_singlethreaded_.
        git_object *obj;
        git_tree_entry_to_object(&obj, curr_repo, it->second);
        string path = pfx + it->first;

        if (git_tree_entry_type(it->second) == GIT_OBJ_TREE) {
            // We only have threads chew through root level directories.
            // And if order is set, then we don't want to be unsure of which
            // tree is going to end up getting walked and posted to
            // files_to_index_ first.
            if (is_singlethreaded_ || depth == 1) {
                walk_tree(path + "/", "", repopath, walk_submodules, submodule_prefix, idx_tree, (git_tree*)obj, curr_repo, 1);
                git_object_free(obj);
                continue;
            }

            // If this is a root tree, add it to the list of trees to walk concurrently
            tree_to_walk *t = new tree_to_walk;
            t->prefix = path + "/";
            t->repopath = repopath;
            t->walk_submodules = walk_submodules;
            t->submodule_prefix = submodule_prefix;
            t->idx_tree = idx_tree;
            t->tree = (git_tree*)obj;
            t->repo = curr_repo;

            trees_to_walk_.push(t);

        } else if (git_tree_entry_type(it->second) == GIT_OBJ_BLOB) {
            const string full_path = submodule_prefix + path;

            pre_indexed_file *file = new pre_indexed_file;
            file->tree = idx_tree;
            file->repopath = repopath;
            file->path = path;
            file->score = score_file(full_path);
            file->repo = curr_repo;
            file->oid = (git_oid *)malloc(sizeof(git_oid));
            git_oid_cpy(file->oid, git_object_id(obj));

            git_object_free(obj); // free the blob

            // This is as fast or faster, believe it or not, than using a queue
            // per thread, then pushing the results onto the global list after
            // each thread is done.
            std::lock_guard<std::mutex> guard(files_mutex_);
            files_to_index_.push_back(file);
        } else if (git_tree_entry_type(it->second) == GIT_OBJ_COMMIT) {
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

            const git_oid* rev = git_tree_entry_id(it->second);
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

            std::lock_guard<std::mutex> guard(files_mutex_);
            open_git_repos_.push_back(sub_repo);
        }
    }
}
