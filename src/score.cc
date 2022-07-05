/********************************************************************
 * livegrep -- score.cc
 * Copyright (c) 2022 Rodrigo Silva Mendoza
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include <stdio.h>
#include <string.h>

#include <string>

#include "re2/re2.h"

#include "src/score.h"

using re2::RE2;
using std::string;

static RE2 generated_re("min\\.(js|css)|js\\.map|_pb2\\/|generated|minified|bundle\\.|package-lock\\.json|yarn\\.lock|go\\.sum");
static RE2 vendor_re("(node_modules|vendor|github.com|third_party|thirdparty|external)\\/");
static RE2 test_re("test|\\.spec\\.jsx");

int score_file(const string& file_path) {
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
    // 1. Promote files that have many references to that file. E.g PageRank.
    //    This would be difficult to do with regex statements
    //    for the many types of imports that are possible.
    //    Also, this requires "reading" all of the content of all
    //    files to be indexed before-hand, which will turn into a scale/compute
    //    problem when using this on a large enough number of repos.
    // 2. What else? Zoekt uses file content length and file name, but with our
    //    absence of ctags symbols, that might skew results too much.
    // 3. Number of ctags symbols? Don't yet have ctags however.

    return starting_score;
}


