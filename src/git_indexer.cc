#include <gflags/gflags.h>
#include <sstream>

#include "src/lib/metrics.h"
#include "src/lib/debug.h"
#include "src/lib/threadsafe_progress_indicator.h"

#include "src/codesearch.h"
#include "src/git_indexer.h"
#include "src/smart_git.h"
#include "src/score.h"

#include "src/proto/config.pb.h"

using namespace std;
using namespace std::chrono;

DEFINE_string(order_root, "", "Walk top-level directories in this order.");
DEFINE_bool(revparse, false, "Display parsed revisions, rather than as-provided");

git_indexer::git_indexer(code_searcher *cs,
                         const google::protobuf::RepeatedPtrField<RepoSpec>& repositories)
    : cs_(cs), repositories_to_index_(repositories), repositories_to_index_length_(repositories.size()), trees_to_walk_(), fq_() {
    int err;
    if ((err = git_libgit2_init()) < 0)
        die("git_libgit2_init: %s", giterr_last()->message);

    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_BLOB, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_OFS_DELTA, 10*1024);
    git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJ_REF_DELTA, 10*1024);
}

git_indexer::~git_indexer() {
    fprintf(stderr, "Closing open git repos\n");
    for (auto it = open_git_repos_.begin(); it != open_git_repos_.end(); ++it) {
        git_repository_free(*(it));
    }
    git_libgit2_shutdown();
}


// Used to get the next index of a repo a thread should focus on
int git_indexer::get_next_repo_idx() {
    if (next_repo_to_process_idx_.load() == repositories_to_index_length_) {
        return -1;
    }
    return next_repo_to_process_idx_.fetch_add(1, std::memory_order_relaxed);
}

void git_indexer::print_last_git_err_and_exit(int err) {
    const git_error *e = giterr_last();
    printf("Error %d/%d: %s\n", err, e->klass, e->message);
    exit(1);
}

/* void git_indexer::process_repos(int estimatedReposToProcess, threadsafe_progress_indicator *tpi) { */
/*     int idx_to_process = get_next_repo_idx(); */
/*     std::vector<git_repository *> open_git_repos_local; */
/*     std::vector<std::unique_ptr<pre_indexed_file>> files_to_index_local; */

/*     open_git_repos_local.reserve(estimatedReposToProcess); */
/*     files_to_index_local.reserve(estimatedReposToProcess * 100); */

/*     while (idx_to_process >= 0) { */
/*         /1* fprintf(stderr, "going to process: %d\n", idx_to_process); *1/ */
/*         /1* auto start = high_resolution_clock::now(); *1/ */
/*         const auto &repo = repositories_to_index_[idx_to_process]; */
/*         const char *repopath = repo.path().c_str(); */

/*         /1* fprintf(stderr, "walking repo: %s\n", repopath); *1/ */
/*         git_repository *curr_repo = NULL; */

/*         // Is it safe to assume these are bare repos (or the mirror clones) */
/*         // that we create?. If so, we can use git_repository_open_bare */
/*         int err = git_repository_open_bare(&curr_repo, repopath); */
/*         if (err < 0) { */
/*             print_last_git_err_and_exit(err); */
/*         } */

/*         open_git_repos_local.push_back(curr_repo); */

/*         /1* for (auto &rev : repo.revisions()) { *1/ */
/*             /1* fprintf(stderr, "walking %s at %s \n", repopath, rev.c_str()); *1/ */
/*             /1* walk(curr_repo, rev, repo.path(), repo.name(), repo.metadata(), repo.walk_submodules(), "", files_to_index_local); *1/ */
/*             /1* fprintf(stderr, "done walking %s at %s\n", repopath, rev.c_str()); *1/ */
/*             /1* tpi->tick(); *1/ */
/*         /1* } *1/ */
/*         /1* auto stop = high_resolution_clock::now(); *1/ */
/*         /1* auto duration = duration_cast<seconds>(stop - start); *1/ */
/*         /1* cout << "took: " << duration.count() << " seconds to process_repos" << endl; *1/ */

/*         idx_to_process = get_next_repo_idx(); */
/*     } */

/*     std::lock_guard<std::mutex> guard(files_mutex_); */
/*     files_to_index_.insert(files_to_index_.end(), std::make_move_iterator(files_to_index_local.begin()), std::make_move_iterator(files_to_index_local.end())); */
/*     open_git_repos_.insert(open_git_repos_.end(), open_git_repos_local.begin(), open_git_repos_local.end()); */
/* } */
void git_indexer::process_trees(int thread_id) {
    fprintf(stderr, "thread %d listening\n", thread_id);
    tree_to_walk *d;

    while (trees_to_walk_.pop(&d)) {
        // got a t with
        /* fprintf(stderr, "thread[%d] popped an elem off the stack\n", thread_id); */
        /* std::cout << t; */

        /* std::cout << "Name: " << d->name << " repopath: " << d->repopath << std::endl; */

        /* std::cout << "repopath: " << d->repopath << std::endl; */
        /* fprintf(stderr, "thread[%d]: %s/%s\n", thread_id, d->repopath.c_str(), d->prefix.c_str()); */

        /* std::cout << "Id addr: " << tree << std::endl; */
              /* << "walk_submodules: " << tree->walk_submodules << std::endl */
              /* << "submodule_prefix: " << tree->submodule_prefix << std::endl */
              /* << "idx_tree.name " << tree->idx_tree->name << std::endl; */
        /* fprintf(stderr, "[%d]: walk_tree('%s','%s','%s')\n", thread_id, tree->prefix.c_str(), tree->order.c_str(), tree->repopath.c_str()); */
        walk_tree(d->prefix, d->order, d->repopath, d->walk_submodules, 
                d->submodule_prefix, d->idx_tree, d->tree, d->repo, 1);
    }
}

/* void git_indexer::process_trees(int thread_id) { */
/*     fprintf(stderr, "thread %d listening\n", thread_id); */
/*     tree_to_walk *tree; */

/*     while (trees_to_walk_.pop(&tree)) { */
/*         // got a t with */
/*         fprintf(stderr, "thread[%d] popped an elem off the stack\n", thread_id); */
/*         /1* std::cout << t; *1/ */
/*         std::cout << "Id: " << tree->id << std::endl */ 
/*               << "Prefix: " << tree->prefix << std::endl */
/*               << "Order: " << tree->order << std::endl */
/*               << "repopath: " << tree->repopath << std::endl;; */
/*         fprintf(stderr, "\n"); */
/*               /1* << "walk_submodules: " << tree->walk_submodules << std::endl *1/ */
/*               /1* << "submodule_prefix: " << tree->submodule_prefix << std::endl *1/ */
/*               /1* << "idx_tree.name " << tree->idx_tree->name << std::endl; *1/ */
/*         /1* fprintf(stderr, "[%d]: walk_tree('%s','%s','%s')\n", thread_id, tree->prefix.c_str(), tree->order.c_str(), tree->repopath.c_str()); *1/ */
/*         walk_tree(tree->prefix, tree->order, tree->repopath, tree->walk_submodules, */ 
/*                 tree->submodule_prefix, tree->idx_tree, tree->tree, tree->repo, 1); */
/*     } */
/* } */

void git_indexer::begin_indexing() {

    // min_per_thread will require tweaking. For example, even with only
    // 2 repos, would it not be worth it to spin up two threads (if available)?
    // Or would the overhead of the thread creation far outweigh the single-core
    // performance for just a few repos.
    
    /* unsigned long const length = repositories_to_index_.size(); */
    /* unsigned long const min_per_thread = 16; */ 
    /* unsigned long const max_threads = */ 
    /*     (length + min_per_thread - 1)/min_per_thread; */
    /* unsigned long const hardware_threads = std::thread::hardware_concurrency(); */
    unsigned long const num_threads = std::thread::hardware_concurrency();

    /* fprintf(stderr, "length=%lu min_per_thread=%lu max_threads=%lu hardware_threads=%lu num_threads=%lu\n", length, min_per_thread, max_threads, hardware_threads, num_threads); */
    /* std::vector<git_repository *> open_git_repos_local; */
    /* std::vector<std::unique_ptr<pre_indexed_file>> files_to_index_local; */

    /* open_git_repos_local.reserve(estimatedReposToProcess); */
    /* files_to_index_local.reserve(estimatedReposToProcess * 100); */

    // we should spin up n number of threads, that listen to a queue of trees_to_walks
    // then walk_tree adds items to the queue whenenver it hits a tree
    // Unfortunately that means we have n producers and n consumers

    // A single repo at a time
    /* int idx = 0; */

    threads_.reserve(num_threads - 1);
    for (long i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&git_indexer::process_trees, this, i);
    }
    threadsafe_progress_indicator tpi(repositories_to_index_length_, "Walking repos...", "Done");
    auto start = high_resolution_clock::now();
     for (const auto &repo : repositories_to_index_) {
        const char *repopath = repo.path().c_str();

        /* fprintf(stderr, "walking repo: %s\n", repopath); */
        git_repository *curr_repo = NULL;

        // Is it safe to assume these are bare repos (or the mirror clones)
        // that we create?. If so, we can use git_repository_open_bare
        int err = git_repository_open_bare(&curr_repo, repopath);
        if (err < 0) {
            print_last_git_err_and_exit(err);
        }

        open_git_repos_.push_back(curr_repo);

        for (auto &rev : repo.revisions()) {
            /* fprintf(stderr, "walking %s at %s \n", repopath, rev.c_str()); */
            walk(curr_repo, rev, repo.path(), repo.name(), repo.metadata(), repo.walk_submodules(), "");
            /* tree_to_walk t{ */
            /*     std::rand(), */
            /*     "/", */
            /*     "", */
            /*     repo.path(), */
            /*     false, */
            /*     "", */
            /* }; */
            /* tree_to_walk t{}; */
            /* t.repopath = repopath; */
            /* t.prefix = path + "/"; */
            
        /* fprintf(stderr, "t is initially ----\n"); */
        /* fprintf(stderr, "repopath.size(): %lu\n", repopath.size()); */
        /* std::cout << "Prefix: " << t.prefix << std::endl */
        /*       << "Order: " << t.order << std::endl */
        /*       << "repopath: " << t.repopath << std::endl */
        /*       << "walk_submodules: " << t.walk_submodules << std::endl */
        /*       << "submodule_prefix: " << t.submodule_prefix << std::endl */
        /*       << "idx_tree.name " << t.idx_tree->name << std::endl; */
            /* fprintf(stderr, "about to push to trees_to_walk_ id: %d\n", t.id); */
            /* trees_to_walk_.push(&t); */
            /* fprintf(stderr, "done walking %s at %s\n", repopath, rev.c_str()); */
        }
        /* tpi.tick(); */
     }

     // ahhh, this may have to be rethought. When we're done walking repos
     // it doesn't mean that all of the subdirectories have been traversed
     // So if we close this, we may run into problems.



    /* if (length < 2 * min_per_thread) { */
    /*     fprintf(stderr, "Not going to create any new threads.\n"); */
    /*     process_repos(length, &tpi); */
    /*     index_files(); */
    /*     return; */
    /* } */

    /* int estimatedReposPerThread = length / num_threads; */
    /* threads_.reserve(num_threads - 1); */
    /* auto start = high_resolution_clock::now(); */
    /* for (long i = 0; i < num_threads; ++i) { */
    /*     threads_.emplace_back(&git_indexer::process_repos, this, estimatedReposPerThread, &tpi); */
    /* } */

    fprintf(stderr, "waiting for threads\n");

    // we can close the trees, since we only add to trees_to_walk_ at the
    // root/unordered level
    trees_to_walk_.close();
    for (long i = 0; i < num_threads; ++i) {
        threads_[i].join();
    }

    // but we can't close the fq_ until all trees have been processed
    fq_.close();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);
    cout << "took: " << duration.count() << " milliseconds to process_repos" << endl;
    /* exit(0); */
    pre_indexed_file *p;


    while (fq_.pop(&p)) {
        /* fprintf(stderr, "pushing back some stuff\n"); */
        files_to_index_.push_back(p);
    }
    fprintf(stderr, "done waiting\n");

    /* exit(0); */

    index_files();
}

bool compareFilesByScore(const std::unique_ptr<pre_indexed_file>& a, const std::unique_ptr<pre_indexed_file>& b) {
    return a->score > b->score;
}

bool compareFilesByTree(const std::unique_ptr<pre_indexed_file>& a, const std::unique_ptr<pre_indexed_file>& b) {
    return a->tree->name < b->tree->name;
}

// sorts `files_to_index_` based on score. This way, the lowest scoring files
// get indexed last, and so show up in sorts results last. We use a stable sort
// so that most of a repos files are indexed together which has 2 benefits:
//  1. If a term is matched in a lot of repos files, that repos files will end
//     up grouped together in search results
//  2. We can avoid many calls to git_repository_open/git_repository_free. At
//     least until the end, where many repos "low" scoring files are found.
// walks `files_to_index_`, looks up the repo & blob combination for each file
// and then calls `cs->index_file` to actually index the file.
void git_indexer::index_files() {
    /* fprintf(stderr, "sorting files_to_index_ by tree... [%lu]\n", files_to_index_.size()); */
    /* std::stable_sort(files_to_index_.begin(), files_to_index_.end(), compareFilesByTree); */
    /* fprintf(stderr, "  done\n"); */

    /* fprintf(stderr, "sorting files_to_index_ by score... [%lu]\n", files_to_index_.size()); */
    /* std::stable_sort(files_to_index_.begin(), files_to_index_.end(), compareFilesByScore); */
    /* fprintf(stderr, "  done\n"); */

    /* fprintf(stderr, "walking files_to_index_ ...\n"); */
    threadsafe_progress_indicator tpi(files_to_index_.size(), "Indexing files_to_index_...", "Done");
    for (auto it = files_to_index_.begin(); it != files_to_index_.end(); ++it) {
        auto file = *it;

        /* fprintf(stderr, "indexing: %s/%s\n", file->repopath.c_str(), file->path.c_str()); */

        git_blob *blob;
        int err = git_blob_lookup(&blob, file->repo, file->oid);

        if (err < 0) {
            print_last_git_err_and_exit(err);
        }
        
        const char *data = static_cast<const char*>(git_blob_rawcontent(blob));
        cs_->index_file(file->tree, file->path, StringPiece(data, git_blob_rawsize(blob)));

        git_blob_free(blob);
        tpi.tick();
    }
}

void git_indexer::walk(git_repository *curr_repo,
        std::string ref, 
        std::string repopath, 
        std::string name,
        Metadata metadata,
        bool walk_submodules,
        std::string submodule_prefix) {
    smart_object<git_commit> commit;
    smart_object<git_tree> tree;
    if (0 != git_revparse_single(commit, curr_repo, (ref + "^0").c_str())) {
        fprintf(stderr, "%s: ref %s not found, skipping (empty repo?)\n", name.c_str(), ref.c_str());
        return;
    }
    git_commit_tree(tree, commit);
    /* fprintf(stderr, "opened commit_tree for %s\n", repopath.c_str()); */

    char oidstr[GIT_OID_HEXSZ+1];
    string version = FLAGS_revparse ?
        strdup(git_oid_tostr(oidstr, sizeof(oidstr), git_commit_id(commit))) : ref;

    const indexed_tree *idx_tree = cs_->open_tree(name, metadata, version);
    walk_tree("", FLAGS_order_root, repopath, walk_submodules, submodule_prefix, idx_tree, tree, curr_repo, 0);
}


void git_indexer::walk_tree(std::string pfx,
                            std::string order,
                            std::string repopath,
                            bool walk_submodules,
                            std::string submodule_prefix,
                            const indexed_tree *idx_tree,
                            git_tree *tree,
                            git_repository *curr_repo,
                            int depth) {
    /* fprintf(stderr, "[%d] preparing to walk_tree for %s with prefix: %s \n", depth, repopath.c_str(), pfx.c_str()); */
    /* if (depth == 1) { */
    /*     return; */
    /* } */
    map<string, const git_tree_entry *> root;
    vector<const git_tree_entry *> ordered;
    int entries = git_tree_entrycount(tree);
    /* fprintf(stderr, "repo: %s has %d entries\n", repopath.c_str(), entries); */
    for (int i = 0; i < entries; ++i) { // TODO: we could divide each of these repos up by threads
        const git_tree_entry *ent = git_tree_entry_byindex(tree, i);
        root[git_tree_entry_name(ent)] = ent;
    }
    /* fprintf(stderr, "%s/%s filled git_tree_entry map ok\n", repopath.c_str(), pfx.c_str()); */

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
            /* fprintf(stderr, "entry is git_tree\n"); */
            // But more likely, when depth == 1, we could add these to a thread
            // pool. In that way, a repo could "potentially" be walked by 10
            // threads at a time
            /* fprintf(stderr, "going to add repopath with %s\n", repopath.c_str()); */
            if (depth == 1) { // don't add to the thread workload
                walk_tree(path + "/", "", repopath, walk_submodules, submodule_prefix, idx_tree, obj, curr_repo, 1);
                return;
            }

            tree_to_walk *t = new tree_to_walk;
            t->prefix = path + "/";
            t->order = "";
            t->repopath = repopath;
            t->walk_submodules = walk_submodules;
            t->submodule_prefix = submodule_prefix;
            t->idx_tree = idx_tree;

            git_object *obj1; 
            git_tree_entry_to_object(&obj1, curr_repo, *it);
            t->tree = (git_tree *)(obj1);
            t->repo = curr_repo;


            trees_to_walk_.push(t);

            /* auto tree = std::make_unique<tree_to_walk>(); */
            /* tree_to_walk t{ */
            /*     std::rand(), */
            /*     path + "/", */
            /*     "", */
            /*     repopath + "/", */
            /*     walk_submodules, */
            /*     submodule_prefix, */
            /*     idx_tree, */
            /*     obj, */
            /*     curr_repo, */
            /* }; */
            /* tree_to_walk t{}; */
            /* t.repopath = repopath; */
            /* t.prefix = path + "/"; */
            
        /* fprintf(stderr, "t is initially ----\n"); */
        /* fprintf(stderr, "repopath.size(): %lu\n", repopath.size()); */
        /* std::cout << "Prefix: " << t.prefix << std::endl */
        /*       << "Order: " << t.order << std::endl */
        /*       << "repopath: " << t.repopath << std::endl */
        /*       << "walk_submodules: " << t.walk_submodules << std::endl */
        /*       << "submodule_prefix: " << t.submodule_prefix << std::endl */
        /*       << "idx_tree.name " << t.idx_tree->name << std::endl; */
            /* fprintf(stderr, "about to push to trees_to_walk_ id: %d\n", t.id); */
            /* trees_to_walk_.push(&t); */

            /* dummy *d = new dummy; */
            /* d->name = "hello = " + repopath; */
            /* d->repopath = repopath; */
            /* trees_to_walk_.push(d); */

            /* walk_tree(path + "/", "", repopath, walk_submodules, submodule_prefix, idx_tree, obj, curr_repo, results); */
        } else if (git_tree_entry_type(*it) == GIT_OBJ_BLOB) {
            /* fprintf(stderr, "entry is blob\n"); */

            const string full_path = submodule_prefix + path;

            pre_indexed_file *file = new pre_indexed_file;

            file->tree = idx_tree;
            file->repopath = repopath;
            file->path = path;
            file->score = score_file(full_path);
            file->repo = curr_repo;
            file->oid = (git_oid *)malloc(sizeof(git_oid));
            git_oid_cpy(file->oid, git_blob_id(obj));

            /* results.push_back(std::move(file)); */
            /* fprintf(stderr, "about to push to global fq_\n"); */
            fq_.push(file);
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

            int err = git_repository_open_bare(&sub_repo, sub_repopath.c_str());

            if (err < 0) {
                fprintf(stderr, "Unable to open subrepo: %s\n", sub_repopath.c_str());
                print_last_git_err_and_exit(err);
            }

            walk(sub_repo, string(revstr), sub_repopath, string(sub_name), meta, walk_submodules, new_submodule_prefix);

            // TODO: See if this is efficient enough. We're depending on
            // libgi2's shutdown to close these submodule repos, while we
            // manually close those that we append to open_git_repos_.
            /* git_repository_free(sub_repo); */
        }
    }
}
