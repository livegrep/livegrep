#include <string.h>
#include "gtest/gtest.h"

#include "re2/re2.h"

#include "src/tagsearch.h"
#include "src/lib/debug.h"

TEST(tagsearch_test, TagLinesFromQuery) {
    /* All variations on anchoring the beginning and ending of a string. */

    query q = {};
    q.line_pat.reset(new RE2("User"));
    string r = tag_searcher::create_tag_line_regex_from_query(&q);
    ASSERT_EQ(r, "^[^\t]*(User)[^\t]*\t");

    q.line_pat.reset(new RE2("^User"));
    r = tag_searcher::create_tag_line_regex_from_query(&q);
    ASSERT_EQ(r, "^(User)[^\t]*\t");

    q.line_pat.reset(new RE2("User$"));
    r = tag_searcher::create_tag_line_regex_from_query(&q);
    ASSERT_EQ(r, "^[^\t]*(User)\t");

    q.line_pat.reset(new RE2("^User$"));
    r = tag_searcher::create_tag_line_regex_from_query(&q);
    ASSERT_EQ(r, "^(User)\t");

    /* Briefer tests that each subsequent component is (a) correctly
       interpolated, and (b) in at least one case varies how it is
       anchored correctly. */

    q.file_pat.reset(new RE2("models.py"));
    r = tag_searcher::create_tag_line_regex_from_query(&q);
    ASSERT_EQ(r, "^(User)\t[^\t]*(models.py)[^\t]*\t");

    q.file_pat.reset(new RE2("^models.py"));
    r = tag_searcher::create_tag_line_regex_from_query(&q);
    ASSERT_EQ(r, "^(User)\t(models.py)[^\t]*\t");

    q.tags_pat.reset(new RE2("c"));
    r = tag_searcher::create_tag_line_regex_from_query(&q);
    ASSERT_EQ(r, "^(User)\t(models.py)[^\t]*\t\\d+;\"\t.*(c).*$");

    q.file_pat.reset();
    r = tag_searcher::create_tag_line_regex_from_query(&q);
    ASSERT_EQ(r, "^(User)\t[^\t]+\t\\d+;\"\t.*(c).*$");
}
