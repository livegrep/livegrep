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
                       const Metadata &metadata)
    : cs_(cs), repopath_(repopath), name_(name) {
    tree_ = cs->open_tree(name, metadata, "");
}

fs_indexer::~fs_indexer() {
}

void fs_indexer::read_file(const fs::path& path) {
    ifstream in(path.c_str(), ios::in);
    fs::path relpath = fs::relative(path, repopath_);
    cs_->index_file(tree_, relpath.string(), StringPiece(static_cast<stringstream const&>(stringstream() << in.rdbuf()).str().c_str(), fs::file_size(path)));
}

void fs_indexer::walk_contents_file(const fs::path& contents_file_path) {
    ifstream contents_file(contents_file_path.c_str(), ios::in);
    if (!contents_file.is_open()) {
        throw std::ifstream::failure("Unable to open contents file for reading: " + contents_file_path.string());
    }
    string path;
    while (std::getline(contents_file, path)) {
        if (path.length()) {
            read_file(fs::path(repopath_) / path);
        }
    }
}

void fs_indexer::walk(const fs::path& path) {
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
                fs_indexer::walk(itr->path());
            } else if (fs::is_regular_file(itr->status()) ) {
                fs_indexer::read_file(itr->path());
            }
        }
    } else if (fs::is_regular_file(path)) {
        fs_indexer::read_file(path);
    }
}
