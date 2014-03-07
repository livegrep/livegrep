/********************************************************************
 * livegrep -- tools/interface.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_INTERFACE_H
#define CODESEARCH_INTERFACE_H

#include <string>
#include <vector>
#include <stdlib.h>
#include <sys/time.h>

struct match_result;
struct match_stats;
class code_searcher;

class codesearch_interface {
public:
    virtual void build_index(code_searcher *cs, const std::vector<std::string> &argv) = 0;
    virtual void print_match(const match_result *m) = 0;
    virtual void print_error(const std::string &err) = 0;
    virtual void print_prompt(const code_searcher *cs) = 0;
    virtual bool getline(std::string &input) = 0;
    virtual bool parse_query(const std::string &input,
                             std::string &line,
                             std::string &file,
                             std::string &tree) = 0;
    virtual void print_stats(timeval elapsed, const match_stats *stats) = 0;
    virtual void info(const char *msg, ...) = 0;
    virtual ~codesearch_interface();
};

codesearch_interface *make_json_interface(FILE *in, FILE *out);
codesearch_interface *make_cli_interface(FILE *in, FILE *out);

#endif
