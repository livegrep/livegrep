#include <vector>
#include <string.h>

#include "gtest/gtest.h"

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
    std::vector<int> lines_;
    radix_sorter sort_;
};

TEST_F(radix_sorter_test, test_cmp) {
    radix_sorter::cmp_suffix cmp(sort_);
    EXPECT_EQ(cmp(lines_[0], lines_[0]), false);
    EXPECT_EQ(cmp(lines_[0], lines_[1]), true);
    EXPECT_EQ(cmp(lines_[1], lines_[0]), false);
    EXPECT_EQ(cmp(lines_[7], lines_[8]), true);
    EXPECT_EQ(cmp(lines_[7], lines_[9]), true);
    EXPECT_EQ(cmp(lines_[8], lines_[9]), true);

    EXPECT_EQ(cmp(lines_[5], lines_[9]), true);
    EXPECT_EQ(cmp(lines_[9], lines_[5]), false);
    EXPECT_EQ(cmp(lines_[5], lines_[5]), false);
}
