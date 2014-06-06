#include "gtest/gtest.h"
#include <vector>
#include "radix_sorter.h"

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
