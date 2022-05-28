#include <stdio.h>
#include <string.h>

#include <string>

#include "re2/re2.h"

#include "src/rank.h"

using re2::RE2;
using std::string;

static RE2 generated_re("min.js|js.map|_pb2");
static RE2 vendor_re("(node_modules|vendor|vendor|github.com|third_party)\\/");
static RE2 test_re("test");

// We use file_path so we can downrank anything under, say, vendor/*
// or test/*
int rank_file(string file_path) {
    fprintf(stdout, "in rank_file: %s\n", file_path.c_str());

    int starting_score = 0;

    // Check if this is generated code
    if (RE2::PartialMatch(file_path, generated_re)) {
        starting_score -= 100;
    }

    // Check if this is vendor/third_part code
    if (RE2::PartialMatch(file_path, vendor_re)) {
        starting_score -= 100;
    }
    
    // Check if this is test code
    if (RE2::PartialMatch(file_path, test_re)) {
        starting_score -= 50;
    }

    // Positively ranking a file is much harder. Some ideas
    // 1. Promote files that have many references to that file
    //    This would be difficult to do with regex statements
    //    for the many types of imports that are possible. We could
    //    start slowly with Go files for example, and go from there.
    //    This may however result in Go files being ranked ahead of
    //    other languages in the interim..
    // 2. What else? Zoekt uses file content length, but that seems a mistake
    // 3. Number of ctags files? Don't yet have ctags however.

    fprintf(stdout, "giving a score of: %f\n", starting_score);
    return starting_score;
}


