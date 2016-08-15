#include <string.h>
#include "gtest/gtest.h"

#include "src/codesearch.h"
#include "src/content.h"

class codesearch_test : public ::testing::Test {
protected:
    codesearch_test() {
        cs_.set_alloc(make_mem_allocator());
        tree_ = cs_.open_tree("REPO", 0, "REV0");
    }

    code_searcher cs_;
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
    EXPECT_EQ("/data/file1", f->path);
    EXPECT_EQ(tree_, f->tree);

    string content;

    for (auto it = f->content->begin(cs_.alloc());
         it != f->content->end(cs_.alloc()); ++it) {
        content += it->ToString();
        content += "\n";
    }

    EXPECT_EQ(string(file1), content);
}

TEST_F(codesearch_test, NoTrailingNewLine) {
    cs_.index_file(tree_, "/data/file1", "no newline");
    cs_.finalize();

    EXPECT_EQ(1, cs_.end_files() - cs_.begin_files());

    indexed_file *f = *cs_.begin_files();
    EXPECT_EQ("/data/file1", f->path);
    EXPECT_EQ(tree_, f->tree);

    string content;

    for (auto it = f->content->begin(cs_.alloc());
         it != f->content->end(cs_.alloc()); ++it) {
        content += it->ToString();
        content += "\n";
    }

    EXPECT_EQ(string("no newline\n"), content);
}

struct accumulate_matches {
    vector<match_result> *results_;
    accumulate_matches(vector<match_result> *results) : results_(results) {
        results_->clear();
    }

    void operator()(const match_result *m) {
        results_->push_back(*m);
    }
};

TEST_F(codesearch_test, DuplicateLinesInFile) {
    cs_.index_file(tree_, "/data/file1",
                   "line 1\n"
                   "line 1\n"
                   "line 2\n");
    cs_.finalize();

    code_searcher::search_thread search(&cs_);
    match_stats stats;
    query q;
    RE2::Options opts;
    default_re2_options(opts);
    q.line_pat.reset(new RE2("line 1", opts));
    vector<match_result> results;
    search.match(q, accumulate_matches(&results), &stats);

    ASSERT_EQ(2, results.size());
    EXPECT_EQ(1, results[0].lno);
    EXPECT_EQ(2, results[1].lno);
}

TEST_F(codesearch_test, LongLines) {
    string xs = "x";
    for (int i = 0; i < 10; i++)
        xs += xs;

    cs_.index_file(tree_, "/data/file1",
                   string("line 1\n") +
                   string("NEEDLE|this line is over 1024 characters") + xs + string("\n") +
                   string("line 3\n") +
                   string("NEEDLE\n"));
    cs_.finalize();

    code_searcher::search_thread search(&cs_);
    match_stats stats;
    query q;
    RE2::Options opts;
    default_re2_options(opts);
    q.line_pat.reset(new RE2("NEEDLE", opts));
    vector<match_result> results;
    search.match(q, accumulate_matches(&results), &stats);

    ASSERT_EQ(1, results.size());
    EXPECT_EQ(4, results[0].lno);
}


TEST_F(codesearch_test, RestrictFiles) {
    // tree_ is "REPO"
    cs_.index_file(tree_, "/file1", "contents");
    cs_.index_file(tree_, "/file2", "contents");
    // other is "OTHER"
    const indexed_tree *other = cs_.open_tree("OTHER", 0, "REV0");
    cs_.index_file(other, "/file1", "contents");
    cs_.index_file(other, "/file2", "contents");
    cs_.finalize();

    code_searcher::search_thread search(&cs_);
    match_stats stats;
    query q;
    vector<match_result> results;
    RE2::Options opts;
    default_re2_options(opts);

    q.line_pat.reset(new RE2("contents", opts));
    q.file_pat.reset(new RE2("file1", opts));

    search.match(q, accumulate_matches(&results), &stats);
    ASSERT_EQ(2, results.size());
    EXPECT_EQ("/file1", results[0].file->path);
    EXPECT_EQ("/file1", results[1].file->path);

    q.file_pat.reset();
    q.tree_pat.reset(new RE2("REPO", opts));
    search.match(q, accumulate_matches(&results), &stats);
    ASSERT_EQ(2, results.size());
    EXPECT_EQ("REPO", results[0].file->tree->name);
    EXPECT_EQ("REPO", results[1].file->tree->name);

    q.tree_pat.reset();
    q.negate.file_pat.reset(new RE2("file1", opts));
    search.match(q, accumulate_matches(&results), &stats);
    ASSERT_EQ(2, results.size());
    EXPECT_EQ("/file2", results[0].file->path);
    EXPECT_EQ("/file2", results[1].file->path);
}
