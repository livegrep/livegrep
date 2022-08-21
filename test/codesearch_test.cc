#include <string.h>
#include "gtest/gtest.h"

#include "src/codesearch.h"
#include "src/content.h"
#include "src/tools/grpc_server.h"

class codesearch_test : public ::testing::Test {
protected:
    codesearch_test() {
        cs_.set_alloc(make_mem_allocator());
        tree_ = cs_.open_tree("repo", "REV0");
    }

    code_searcher cs_;
    const indexed_tree *tree_;
};

const char *file1 = "The quick brown fox\n" \
    "jumps over the lazy\n\n\n" \
    "dog.\n";

std::vector<std::string> buildHighlightedStringFromBounds(CodeSearchResult matches) {
    // we use the first result's bounds
    std::vector<std::string> pieces;

    int bounds_len = matches.results(0).bounds().size();
    auto line = matches.results(0).line();

    // this is a dup of the logic found in web/src/codesearch/codesearch_ui.js#L192
    int currIdx = 0;
    for (int i = 0; i < bounds_len; i++) {
        auto bound = matches.results(0).bounds(i);  

        if (bound.left() > currIdx) {
            pieces.push_back(line.substr(currIdx, bound.left() - currIdx));
        }

        currIdx = bound.right();

        pieces.push_back("<span>" + line.substr(bound.left(), bound.right() - bound.left()) + "</span>");

        if (i == bounds_len - 1 && currIdx <= line.length()) {
            pieces.push_back(line.substr(currIdx, line.length() - currIdx));
        }
    }

    return pieces;
}

TEST_F(codesearch_test, IndexTest) {
    cs_.index_file(tree_, "/data/file1", file1);
    cs_.finalize();

    EXPECT_EQ(1, cs_.end_files() - cs_.begin_files());

    indexed_file *f = cs_.begin_files()->get();
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

TEST_F(codesearch_test, BadRegex) {
    cs_.index_file(tree_, "/data/file1", file1);
    cs_.finalize();
    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    Query request;
    CodeSearchResult matches;
    request.set_line("(");

    grpc::ServerContext ctx;

    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(!st.ok());
}

TEST_F(codesearch_test, NoTrailingNewLine) {
    cs_.index_file(tree_, "/data/file1", "no newline");
    cs_.finalize();

    EXPECT_EQ(1, cs_.end_files() - cs_.begin_files());

    indexed_file *f = cs_.begin_files()->get();
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

TEST_F(codesearch_test, DuplicateLinesInFile) {
    cs_.index_file(tree_, "/data/file1",
                   "line 1\n"
                   "line 1\n"
                   "line 2\n");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    Query request;
    CodeSearchResult matches;
    request.set_line("line 1");

    grpc::ServerContext ctx;

    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(2, matches.results_size());
    EXPECT_EQ(1, matches.results(0).line_number());
    EXPECT_EQ(2, matches.results(1).line_number());
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

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    Query request;
    CodeSearchResult matches;
    request.set_line("NEEDLE");

    grpc::ServerContext ctx;

    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(1, matches.results_size());
    EXPECT_EQ(4, matches.results(0).line_number());
}


TEST_F(codesearch_test, RestrictFiles) {
    // tree_ is "repo"
    cs_.index_file(tree_, "/file1", "contents");
    cs_.index_file(tree_, "/file2", "contents");
    // other is "OTHER"
    const indexed_tree *other = cs_.open_tree("OTHER", "REV0");
    cs_.index_file(other, "/file1", "contents");
    cs_.index_file(other, "/file2", "contents");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    Query request;
    CodeSearchResult matches;
    grpc::ServerContext ctx;
    grpc::Status st;

    request.set_line("contents");
    request.set_file("file1");

    st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(2, matches.results_size());
    EXPECT_EQ("/file1", matches.results(0).path());
    EXPECT_EQ("/file1", matches.results(1).path());

    request.clear_file();
    request.set_repo("repo");

    matches.Clear();
    st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(2, matches.results_size());
    EXPECT_EQ("repo", matches.results(0).tree());
    EXPECT_EQ("repo", matches.results(1).tree());

    request.clear_repo();
    request.set_not_file("file1");

    matches.Clear();
    st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(2, matches.results_size());
    EXPECT_EQ("/file2", matches.results(0).path());
    EXPECT_EQ("/file2", matches.results(1).path());
}


TEST_F(codesearch_test, Tags) {
    cs_.index_file(tree_,
                   "file.c",
                   "void do_the_thing(void) {\n"
                   "}\n"
                   "do_the_thing()\n");
    cs_.finalize();

    code_searcher tags;
    tags.set_alloc(make_mem_allocator());
    const indexed_tree *tag_tree = cs_.open_tree("", "HEAD");
    tags.index_file(tag_tree,
                    "tags",
                    "do_the_thing\trepo/file.c\t1;\"\tfunction\n");
    tags.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, &tags, nullptr));
    Query request;
    CodeSearchResult matches;
    grpc::ServerContext ctx;
    grpc::Status st;

    request.set_line("do_the_thing");
    request.set_tags("func");

    st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(1, matches.results_size());
}


TEST_F(codesearch_test, MaxMatches) {
    cs_.index_file(tree_, "/file1", "contents");
    cs_.index_file(tree_, "/file2", "contents");
    cs_.index_file(tree_, "/file3", "contents");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    {
        CodeSearchResult all_matches;
        Query request;
        request.set_line("contents");
        request.set_max_matches(0);
        grpc::ServerContext ctx;
        grpc::Status st = srv->Search(&ctx, &request, &all_matches);
        ASSERT_TRUE(st.ok());
        ASSERT_EQ(3, all_matches.results_size());
    }
    {
        CodeSearchResult limited_matches;
        Query request;
        request.set_line("contents");
        request.set_max_matches(2);
        grpc::ServerContext ctx;
        grpc::Status st = srv->Search(&ctx, &request, &limited_matches);
        ASSERT_TRUE(st.ok());
        ASSERT_EQ(request.max_matches(), limited_matches.results_size());
    }
}

TEST_F(codesearch_test, LineCaseAndFileCaseAreIndependent) {
    cs_.index_file(tree_, "/file1", "contents");
    cs_.index_file(tree_, "/FILE2", "CONTENTS");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    {
        CodeSearchResult matches;
        Query request;
        request.set_line("c");
        request.set_fold_case(true);
        request.set_file("FILE1");
        grpc::ServerContext ctx;
        grpc::Status st = srv->Search(&ctx, &request, &matches);
        ASSERT_TRUE(st.ok());
        ASSERT_EQ(0, matches.results_size());
    }
    {
        CodeSearchResult matches;
        Query request;
        request.set_line("CONTENTS");
        request.set_fold_case(false);
        request.set_file("file2");
        grpc::ServerContext ctx;
        grpc::Status st = srv->Search(&ctx, &request, &matches);
        ASSERT_TRUE(st.ok());
        ASSERT_EQ(1, matches.results_size());
    }
}

TEST_F(codesearch_test, LineCaseAndRepoCaseAreIndependent) {
    const indexed_tree *other = cs_.open_tree("OTHER", "REV0");
    cs_.index_file(tree_, "/file1", "contents");
    cs_.index_file(other, "/file1", "CONTENTS");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    {
        CodeSearchResult matches;
        Query request;
        request.set_line("c");
        request.set_fold_case(true);
        request.set_repo("REPO");
        grpc::ServerContext ctx;
        grpc::Status st = srv->Search(&ctx, &request, &matches);
        ASSERT_TRUE(st.ok());
        ASSERT_EQ(0, matches.results_size());
    }
    {
        CodeSearchResult matches;
        Query request;
        request.set_line("CONTENTS");
        request.set_fold_case(false);
        request.set_repo("other");
        grpc::ServerContext ctx;
        grpc::Status st = srv->Search(&ctx, &request, &matches);
        ASSERT_TRUE(st.ok());
        ASSERT_EQ(1, matches.results_size());
    }
}

TEST_F(codesearch_test, FilenameTest) {
    cs_.index_file(tree_, "/file1", "contents");
    cs_.index_file(tree_, "/file2", "mention of file1");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    CodeSearchResult matches;
    Query request;
    request.set_line("file1");
    grpc::ServerContext ctx;
    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());
    ASSERT_EQ(1, matches.results_size());
    ASSERT_EQ(1, matches.file_results_size());
    ASSERT_EQ("/file1", matches.file_results(0).path());
}

TEST_F(codesearch_test, FilenameWithIndexBoundaryTest) {
    std::string last_file;
    for (int i = 0; i < 1000; i++) {
        std::string name = "file" + std::to_string(i);
        cs_.index_file(tree_, name, name);
    }
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    CodeSearchResult matches;
    Query request;
    request.set_line("ile999"); // intentionally chosen to match in the middle
    grpc::ServerContext ctx;
    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());
    ASSERT_EQ(1, matches.results_size());
    ASSERT_EQ(1, matches.file_results_size());
    ASSERT_EQ("file999", matches.file_results(0).path());
}

TEST_F(codesearch_test, FilenameDoubleMatchTest) {
    for (int i=0; i < 200; i++) {
        // Drive the index size high enough that "count > indexes->size()"
        cs_.index_file(tree_, std::string("/abcd") + std::to_string(i), "hat");
    }
    cs_.index_file(tree_, "/filename", "cat");  // "e" twice in filename
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    CodeSearchResult matches;
    Query request;
    request.set_line("e");      // but should only get file returned once
    grpc::ServerContext ctx;
    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());
    ASSERT_EQ(0, matches.results_size());
    ASSERT_EQ(1, matches.file_results_size());
    ASSERT_EQ("/filename", matches.file_results(0).path());
}

TEST_F(codesearch_test, FilenameOnlyTest) {
    cs_.index_file(tree_, "/file1", "contents");
    cs_.index_file(tree_, "/file2", "mention of file1");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    CodeSearchResult matches;
    Query request;
    request.set_line("file1");
    request.set_filename_only(true);
    grpc::ServerContext ctx;
    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());
    ASSERT_EQ(0, matches.results_size());
    ASSERT_EQ(1, matches.file_results_size());
    ASSERT_EQ("/file1", matches.file_results(0).path());
}

TEST_F(codesearch_test, BadUTF8) {
    cs_.index_file(tree_, "/data/file1",
                   "line 0\xe9\n"
                   "line 1\n"
                   "line 2\n");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    Query request;
    CodeSearchResult matches;
    request.set_line("line 1");

    grpc::ServerContext ctx;

    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(1, matches.results_size());
    EXPECT_EQ(2, matches.results(0).line_number());
    ASSERT_EQ(1, matches.results(0).context_before().size());
    EXPECT_EQ("<invalid utf-8>",
              matches.results(0).context_before(0));

    matches.Clear();
    request.set_line("line 0");
    st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());
    ASSERT_EQ(0, matches.results_size());
}

TEST_F(codesearch_test, AllMatchesOnLineFound) {
    cs_.index_file(tree_, "/file1", "test --test-timeout=60s");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    Query request;
    CodeSearchResult matches;
    request.set_line("test");
    
    grpc::ServerContext ctx;

    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(1, matches.results_size());
    ASSERT_EQ(2, matches.results(0).bounds().size());
    ASSERT_EQ(2, matches.stats().num_matches());

    // bounds should be [[0, 4], [7, 11]]
    auto first_bound = matches.results(0).bounds(0);
    auto second_bound = matches.results(0).bounds(1);

    ASSERT_EQ(0, first_bound.left());
    ASSERT_EQ(4, first_bound.right());

    ASSERT_EQ(7, second_bound.left());
    ASSERT_EQ(11, second_bound.right());
}

TEST_F(codesearch_test, ConsecutiveMatchBoundsMerged) {
    cs_.index_file(tree_, "/file1", "ttt merge");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    Query request;
    CodeSearchResult matches;
    request.set_line("t");
    
    grpc::ServerContext ctx;

    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(1, matches.results_size());
    ASSERT_EQ(1, matches.results(0).bounds().size());
    ASSERT_EQ(3, matches.stats().num_matches());

    auto first_bound = matches.results(0).bounds(0);
    ASSERT_EQ(0, first_bound.left());
    ASSERT_EQ(3, first_bound.right());
}

TEST_F(codesearch_test, WOperatorWithRepetition) {
    cs_.index_file(tree_, "/file1", "there should be five words");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    Query request;
    CodeSearchResult matches;
    request.set_line("(\\w+)");
    
    grpc::ServerContext ctx;

    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(1, matches.results_size());
    ASSERT_EQ(5, matches.results(0).bounds().size());
    ASSERT_EQ(5, matches.stats().num_matches());


    std::vector<std::vector<int>> v = {{0,5}, {6,12}, {13,15}, {16,20}, {21, 26}}; 

    for (int i = 0; i < v.size(); i++) {
        auto bound = matches.results(0).bounds(i);
        ASSERT_EQ(v[i][0], bound.left());
        ASSERT_EQ(v[i][1], bound.right());
    }

    // Now that we attempt to find all matches on a line, no matter the input,
    // any regex operator will act as if it wrapped in a repetition operator
    // so, for example, (\w) should act the same as (\w+) - at a high level
    matches.Clear();
    request.set_line("(\\w)");
    st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(1, matches.results_size());
    // bounds will be the same size as before, because we merge consecutive
    ASSERT_EQ(5, matches.results(0).bounds().size());
    // however, since we're matching an inidivudal \w at a time, we'll have way
    // more matches than before (5)
    ASSERT_EQ(22, matches.stats().num_matches());

    for (int i = 0; i < v.size(); i++) {
        auto bound = matches.results(0).bounds(i);
        ASSERT_EQ(v[i][0], bound.left());
        ASSERT_EQ(v[i][1], bound.right());
    }
}

TEST_F(codesearch_test, UnicodeLines) {
    cs_.index_file(tree_, "/file1", 
            "/ \u21B4 / /\n"
            "\u25BC\n"
            "line 3");
    cs_.finalize();

    std::unique_ptr<CodeSearch::Service> srv(build_grpc_server(&cs_, nullptr, nullptr));
    Query request;
    CodeSearchResult matches;
    request.set_line("/");
    
    grpc::ServerContext ctx;

    grpc::Status st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());

    ASSERT_EQ(1, matches.results_size());
    ASSERT_EQ(1, matches.results(0).line_number());
    ASSERT_EQ(3, matches.results(0).bounds().size());
    ASSERT_EQ(3, matches.stats().num_matches());


    // but again, bounds are the same as before because of bounds merging
    std::vector<std::vector<int>> v = {{0,1}, {6,7}, {8,9}}; 

    for (int i = 0; i < v.size(); i++) {
        auto bound = matches.results(0).bounds(i);
        ASSERT_EQ(v[i][0], bound.left());
        ASSERT_EQ(v[i][1], bound.right());
    }

    // now, we take our first line, and make sure we can reconstruct
    // a "highlighted" version of it
    std::vector<std::string> expected = {"<span>/</span>", " \u21B4 ", "<span>/</span>", " ", "<span>/</span>", ""};
    std::vector<std::string> pieces = buildHighlightedStringFromBounds(matches);
    ASSERT_EQ(expected, pieces);


    // --------------------------
    matches.Clear();
    request.set_line("\u25BC");
    st = srv->Search(&ctx, &request, &matches);
    ASSERT_TRUE(st.ok());


    ASSERT_EQ(1, matches.results_size());
    ASSERT_EQ(2, matches.results(0).line_number());
    ASSERT_EQ(1, matches.results(0).bounds().size());

    ASSERT_EQ(0, matches.results(0).bounds(0).left());
    ASSERT_EQ(1, matches.results(0).bounds(0).right());

}
