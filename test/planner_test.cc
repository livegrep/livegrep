#include <string.h>
#include "gtest/gtest.h"

#include "re2/re2.h"

#include "src/codesearch.h"
#include "src/query_planner.h"
#include "src/lib/debug.h"

TEST(QueryPlanTest, BasicCaseFold) {
    re2::RE2::Options opts;
    opts.set_case_sensitive(false);

    re2::RE2 re("k", opts);
    intrusive_ptr<QueryPlan> key = constructQueryPlan(re);

    ASSERT_EQ(3, key->size());
    QueryPlan::iterator it = key->begin();
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

TEST(QueryPlanTest, Alternate) {
    re2::RE2::Options opts;
    opts.set_case_sensitive(false);

    re2::RE2 re("(se|in)_", opts);
    intrusive_ptr<QueryPlan> key = constructQueryPlan(re);
    EXPECT_TRUE(key->anchor & kAnchorRight);
    list<QueryPlan::const_iterator> tails;
    key->collect_tails(tails);
    EXPECT_EQ(1, tails.size());
}

TEST(QueryPlanTest, AlternateIndef) {
    re2::RE2::Options opts;
    opts.set_case_sensitive(false);

    re2::RE2 re("(se|in).", opts);
    intrusive_ptr<QueryPlan> key = constructQueryPlan(re);
    EXPECT_FALSE(key->anchor & kAnchorRight);
}

TEST(QueryPlanTest, CaseFoldRegression) {
    re2::RE2::Options opts;
    opts.set_case_sensitive(false);

    re2::RE2 re("ksp", opts);
    intrusive_ptr<QueryPlan> key = constructQueryPlan(re);
    EXPECT_TRUE(key->anchor & kAnchorLeft);
    EXPECT_TRUE(key->anchor & kAnchorRight);
}

TEST(QueryPlanTest, LongCaseFoldedLiteral) {
    re2::RE2::Options opts;
    default_re2_options(opts);
    opts.set_case_sensitive(false);

    re2::RE2 re("sxxxxxxxxxxxxxxxxxxxxxxxxx", opts);
    intrusive_ptr<QueryPlan> key = constructQueryPlan(re);
    EXPECT_TRUE(key);
    EXPECT_GT(key->depth(), 2);
}

TEST(QueryPlanTest, StressTest) {
    const char *cases[] = {
        "([a-e]:)|[g-k]",
        "([a-e]:)|[a-e]",
        "(([a-e]:)|[a-e])n",
        "([a-e]:)|[d-g]",
        "([a-e]:)|([d-g];)",
        "([a-ei-lz]:)|([d-gl-oy];)",
        "\\s[0-9a-f]",
        "[a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k][a-bd-eg-hj-k]",
        "([a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z][a-z])|([a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z][a-fw-z])",
        "([acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}])|([acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}][acegikmoqsuwy{}])",
        "((p((n((l((j((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))|(k((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))))|(m((j((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))|(k((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))))))|(o((l((j((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))|(k((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))))|(m((j((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))|(k((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))))))))|(q((n((l((j((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))|(k((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))))|(m((j((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))|(k((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))))))|(o((l((j((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))|(k((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))))|(m((j((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))))|(k((h((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b))))))|(i((f((d(a|b))|(e(a|b))))|(g((d(a|b))|(e(a|b)))))))))))))))",
        "([aA]|[bB])cdefg",
        "[sS][tT][aA][cC][kK]_",
    };
    re2::RE2::Options opts;

    for (unsigned int i = 0; i < sizeof(cases)/sizeof(*cases); ++i) {
        const char *pat = cases[i];
        re2::RE2 re(pat, opts);
        intrusive_ptr<QueryPlan> key = constructQueryPlan(re);
        EXPECT_TRUE(key) << "could not compute key for: " << pat;
    }
}
