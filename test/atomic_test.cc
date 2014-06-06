#include "gtest/gtest.h"

#include "atomic.h"

TEST(atomic, BasicInt) {
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

TEST(atomic, Long) {
    atomic_long i;
    EXPECT_EQ(0, i.load());

    i += (1ul << 40);
    EXPECT_EQ((1ul << 40), i.load());
}
