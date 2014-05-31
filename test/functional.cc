#include "gtest/gtest.h"
#include "codesearch.h"
#include "content.h"

class codesearch_test : public ::testing::Test {
protected:
    codesearch_test() {
        cs_.set_alloc(make_mem_allocator());
        repo_ = cs_.open_repo("REPO", 0);
        tree_ = cs_.open_revision(repo_, "REV0");
    }

    code_searcher cs_;
    const indexed_repo *repo_;
    const indexed_tree *tree_;
};

const char *file1 = "The quick brown fox\n" \
    "jumps over the lazy\n\n\n" \
    "dog.\n";

TEST_F(codesearch_test, IndexTest) {
    cs_.index_file(tree_, "/data/file1", file1);
    cs_.finalize();

    EXPECT_EQ(1, cs_.end_files() - cs_.begin_files());

    indexed_file *f = *cs_.begin_files();
    EXPECT_EQ("/data/file1", f->paths[0].path);
    EXPECT_EQ(tree_, f->paths[0].tree);

    string content;

    for (auto it = f->content->begin(cs_.alloc());
         it != f->content->end(cs_.alloc()); ++it) {
        content += it->ToString();
        content += "\n";
    }

    EXPECT_EQ(string(file1), content);
}
