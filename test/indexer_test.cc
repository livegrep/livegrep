#include <string.h>
#include "gtest/gtest.h"

#include "re2/re2.h"

#include "src/indexer.h"
#include "src/lib/debug.h"

TEST(IndexKeyTest, BasicCaseFold) {
    re2::RE2::Options opts;
    opts.set_case_sensitive(false);

    re2::RE2 re("k", opts);
    intrusive_ptr<IndexKey> key = indexRE(re);

    ASSERT_EQ(3, key->size());
    IndexKey::iterator it = key->begin();
    EXPECT_EQ('K', it->first.first);
    EXPECT_EQ('K', it->first.second);
    EXPECT_FALSE(it->second);
    ++it;
    EXPECT_EQ('k', it->first.first);
    EXPECT_EQ('k', it->first.second);
    EXPECT_FALSE(it->second);
    ++it;
    // U+212A KELVIN SIGN aka [e2 84 aa]
    EXPECT_EQ(0xe2, it->first.first);
    EXPECT_EQ(0xe2, it->first.second);
    EXPECT_TRUE(it->second);
}

TEST(IndexKeyTest, Alternate) {
    re2::RE2::Options opts;
    opts.set_case_sensitive(false);

    re2::RE2 re("(se|in)_", opts);
    intrusive_ptr<IndexKey> key = indexRE(re);
    EXPECT_TRUE(key->anchor & kAnchorRight);
    list<IndexKey::const_iterator> tails;
    key->collect_tails(tails);
    EXPECT_EQ(1, tails.size());
}

TEST(IndexKeyTest, AlternateIndef) {
    re2::RE2::Options opts;
    opts.set_case_sensitive(false);

    re2::RE2 re("(se|in).", opts);
    intrusive_ptr<IndexKey> key = indexRE(re);
    EXPECT_FALSE(key->anchor & kAnchorRight);
}

TEST(IndexKeyTest, CaseFoldRegression) {
    re2::RE2::Options opts;
    opts.set_case_sensitive(false);

    re2::RE2 re("ksp", opts);
    intrusive_ptr<IndexKey> key = indexRE(re);
    EXPECT_TRUE(key->anchor & kAnchorLeft);
    EXPECT_TRUE(key->anchor & kAnchorRight);
}
