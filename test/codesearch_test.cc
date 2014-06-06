#include <vector>
#include <string.h>

#include "gtest/gtest.h"

#include "radix_sorter.h"
#include "codesearch.h"
#include "content.h"
#include "atomic.h"

class radix_sorter_test : public ::testing::Test {
protected:
    radix_sorter_test() :
        data_("line 1\n"
              "line 2\n"
              "another line\n"
              "another line\n"
              "line 3\n"
              "\n"
              "int main()\n"
              "really long line that is definitely longer than the word size\n"
              "really long line that is definitely longer than the word size and then some\n"
              "really long line that is definitely longer than the word size like whoa\n"),
        sort_(reinterpret_cast<const unsigned char*>(data_), strlen(data_)) {
        const char *p = data_;
        while (p) {
            lines_.push_back(p - data_);
            p  = strchr(p, '\n');
            if (p) p++;
        }
    }

    const char *data_;
    std::vector<uint32_t> lines_;
    radix_sorter sort_;
};

TEST_F(radix_sorter_test, test_cmp) {
    radix_sorter::cmp_suffix cmp(sort_);
    struct {
        uint32_t lhs, rhs;
        int cmp;
    } tests[] = {
        { lines_[0], lines_[0], 0 },
        { lines_[0], lines_[1], -1 },
        { lines_[7], lines_[8], -1 },
        { lines_[7], lines_[9], -1 },
        { lines_[8], lines_[9], -1 },
        { lines_[5], lines_[9], -1 },
        { lines_[5], lines_[5], 0 },
        { lines_[0] + 6, lines_[5], 0 },
    };

    for (auto it = &tests[0]; it != &(tests + 1)[0]; ++it) {
        EXPECT_EQ(cmp(it->lhs, it->rhs), it->cmp < 0) <<
            "Expected " << it->lhs << " " << (it->cmp < 0 ? "<" : ">") << " " << it->rhs;
        EXPECT_EQ(cmp(it->rhs, it->lhs), it->cmp > 0) <<
            "Expected " << it->rhs << " " << (it->cmp > 0 ? "<" : ">") << " " << it->lhs;
    }
}

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

TEST(atomic_int, Basic) {
    atomic_int i;
    EXPECT_EQ(0, i.load());
    EXPECT_EQ(1, ++i);
    EXPECT_EQ(1, i.load());
    EXPECT_EQ(10, i += 9);
    EXPECT_EQ(10, i.load());
    EXPECT_EQ(9, --i);
    EXPECT_EQ(9, i.load());
    EXPECT_EQ(4, i -= 5);

    atomic_int j(42);
    EXPECT_EQ(42, j.load());
}
