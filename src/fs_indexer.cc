#include <algorithm>
#include <gflags/gflags.h>
#include <sstream>
#include <iostream>

#include "src/lib/recursion.h"

#include "src/codesearch.h"
#include "src/fs_indexer.h"
#include <boost/filesystem.hpp>

static int kMaxRecursion = 100;

using namespace std;
namespace fs = boost::filesystem;

fs_indexer::fs_indexer(code_searcher *cs,
                       const string& repopath,
                       const string& name,
                       json_object *metadata)
    : cs_(cs), repopath_(repopath), name_(name) {
    tree_ = cs->open_tree(name, metadata, "");
}

fs_indexer::~fs_indexer() {
}

void fs_indexer::read_file(const string& path) {
    ifstream in(path.c_str(), ios::in);
    string relpath(mismatch(path.begin(), path.end(), repopath_.begin()).first,
                   path.end());
    cs_->index_file(tree_, relpath, StringPiece(static_cast<stringstream const&>(stringstream() << in.rdbuf()).str().c_str(), fs::file_size(path)));
}

void fs_indexer::walk(const string& path) {
    static int recursion_depth = 0;
    RecursionCounter guard(recursion_depth);
    if (recursion_depth > kMaxRecursion)
        return;
    if (!fs::exists(path)) return;
    fs::directory_iterator end_itr;
    if (fs::is_directory(path)) {
        for (fs::directory_iterator itr(path);
                itr != end_itr;
                ++itr) {
            if (fs::is_directory(itr->status()) ) {
                fs_indexer::walk(itr->path().c_str());
            } else if (fs::is_regular_file(itr->status()) ) {
                fs_indexer::read_file(itr->path().c_str());
            }
        }
    } else if (fs::is_regular_file(path)) {
        fs_indexer::read_file(path);
    }
}
