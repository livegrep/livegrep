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
