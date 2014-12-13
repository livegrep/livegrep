/********************************************************************
 * livegrep -- tools/transport.cc
 * Copyright (c) 2011-2014 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "codesearch.h"
#include "transport.h"
#include "debug.h"

#include <json/json.h>

#include <gflags/gflags.h>

#include <re2/re2.h>
using re2::RE2;
using std::unique_ptr;

namespace {

json_object *to_json(const string &str) {
    return json_object_new_string(str.c_str());
}

json_object *to_json(const StringPiece &str) {
    return json_object_new_string_len(str.data(),
                                      str.size());
}

json_object *to_json(int i) {
    return json_object_new_int(i);
}

json_object *to_json(const indexed_tree &tree) {
    json_object *out = json_object_new_object();
    json_object_object_add(out, "name", to_json(tree.name));
    json_object_object_add(out, "version", to_json(tree.version));
    if (tree.metadata)
        json_object_object_add(out, "metadata", json_object_get(tree.metadata));
    return out;
}

template <class T>
json_object *to_json(vector<T> vec) {
    json_object *out = json_object_new_array();
    for (auto it = vec.begin(); it != vec.end(); it++)
        json_object_array_add(out, to_json(*it));
    return out;
}

json_object *to_json(const index_info *info) {
    json_object *out = json_object_new_object();
    json_object_object_add(out, "name", to_json(info->name));
    json_object_object_add(out, "trees", to_json(info->trees));
    return out;
}

long timeval_ms (struct timeval tv) {
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

json_object *to_json(timeval t) {
    return json_object_new_int(timeval_ms(t));
}

json_object *to_json(const match_stats *stats) {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "re2_time", to_json(stats->re2_time));
    json_object_object_add(obj, "git_time",  to_json(stats->git_time));
    json_object_object_add(obj, "sort_time", to_json(stats->sort_time));
    json_object_object_add(obj, "index_time", to_json(stats->index_time));
    json_object_object_add(obj, "analyze_time", to_json(stats->analyze_time));
    switch(stats->why) {
    case kExitNone: break;
    case kExitMatchLimit:
        json_object_object_add(obj, "why", json_object_new_string("limit"));
        break;
    case kExitTimeout:
        json_object_object_add(obj, "why", json_object_new_string("timeout"));
        break;
    }
    return obj;
}

json_object *json_frame(const std::string op, json_object *body) {
    json_object *frame = json_object_new_object();
    json_object_object_add(frame, "opcode", to_json(op));
    json_object_object_add(frame, "body", body);
    return frame;
}

json_object *json_info(const code_searcher *cs) {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "trees", to_json(cs->trees()));
    json_object_object_add(obj, "name", to_json(cs->name()));
    return obj;
}

bool getline(std::string &out, FILE *in) {
    char *line = 0;
    size_t n = 0;
    ssize_t r;
    r = getline(&line, &n, in);
    if (r == 0 || r == -1)
        out.clear();
    else
        out.assign(line, r - 1);

    return (r != -1) && !feof(in) && !ferror(in) ;
}

struct json_parse_error {
    json_parse_error() : ok_(true) {}
    json_parse_error(const std::string &err) : ok_(false), error(err) {}

    bool ok() {
        return ok_;
    }

    std::string err() {
        if (path.size()) {
            return path + ": " + error;
        } else {
            return error;
        }
    }

    json_parse_error wrap(string path) {
        json_parse_error wrapped = *this;
        if (wrapped.path.size()) {
            wrapped.path = path + "." + wrapped.path;
        } else {
            wrapped.path = path;
        }
        return wrapped;
    }
private:
    bool ok_;
    std::string error;
    std::string path;
};

json_parse_error parse_object(json_object *j, bool *);
json_parse_error parse_object(json_object *j, std::string *);
json_parse_error parse_object(json_object *j, repo_spec *);
json_parse_error parse_object(json_object *j, json_object **);

template <class T>
json_parse_error parse_object(json_object *j, std::vector<T> *);
template<class T>
json_parse_error parse_object(json_object *j, const char *key, T *t);

json_parse_error parse_object(json_object *j, bool *b) {
    if (json_object_get_type(j) != json_type_boolean)
        return json_parse_error("expected boolean");
    *b = json_object_get_boolean(j);
    return json_parse_error();
}

json_parse_error parse_object(json_object *j, std::string *s) {
    if (json_object_get_type(j) != json_type_string)
        return json_parse_error("expected string");
    *s = json_object_get_string(j);
    return json_parse_error();
}

template <class T>
json_parse_error parse_object(json_object *j, std::vector<T> *out) {
    if (json_object_get_type(j) != json_type_array)
        return json_parse_error("expected array");
    for (int i = 0; i < json_object_array_length(j); ++i) {
        json_object *elt = json_object_array_get_idx(j, i);
        T t;
        json_parse_error err = parse_object(elt, &t);
        if (!err.ok())
            return err;
        out->push_back(t);
    }
    return json_parse_error();
}

template<class T>
json_parse_error parse_object(json_object *j, const char *key, T *t) {
    json_object *o = json_object_object_get(j, key);
    if (o == NULL)
        return json_parse_error();
    json_parse_error err = parse_object(o, t);
    if (!err.ok())
        return err.wrap(key);
    return json_parse_error();
}

json_parse_error parse_regex(json_object *js, const char *key,
                             const RE2::Options &opts, unique_ptr<RE2> *out) {
    std::string str;
    json_parse_error err;
    err = parse_object(js, key, &str);
    if (!err.ok())
        return err;
    unique_ptr<RE2> re(new RE2(str, opts));
    if (!re->ok()) {
        return json_parse_error(re->error()).wrap(key);
    }
    *out = std::move(re);
    return json_parse_error();
}

json_parse_error parse_object(json_object *js, query *q) {
    if (json_object_get_type(js) != json_type_object)
        return json_parse_error("expected a JSON object");
    json_object *b = json_object_object_get(js, "body");
    if (b == NULL || json_object_get_type(b) != json_type_object)
        return json_parse_error("expected a JSON object").wrap("body");
    RE2::Options opts;
    default_re2_options(opts);
    bool fold_case = false;
    json_parse_error err = parse_object(b, "fold_case", &fold_case);
    if (!err.ok())
        return err;
    opts.set_case_sensitive(!fold_case);
    err = parse_regex(b, "line", opts, &q->line_pat);

    // file and repo are always case-sensitive
    opts.set_case_sensitive(false);

    if (err.ok())
        err = parse_regex(b, "file", opts, &q->file_pat);
    if (err.ok())
        err = parse_regex(b, "repo", opts, &q->tree_pat);
    return err;
}

json_parse_error parse_object(json_object *js, json_object **out) {
    if (json_object_get_type(js) != json_type_object)
        return json_parse_error("expected a JSON object");
    *out = json_object_get(js);
    return json_parse_error();
}

json_parse_error parse_object(json_object *js, repo_spec *r) {
    if (json_object_get_type(js) != json_type_object)
        return json_parse_error("expected a JSON object");
    json_parse_error err;
    err = parse_object(js, "path", &r->path);
    if (!err.ok()) return err;
    err = parse_object(js, "name", &r->name);
    if (!err.ok()) return err;
    err = parse_object(js, "metadata", &r->metadata);
    if (!err.ok()) return err;
    err = parse_object(js, "revisions", &r->revisions);
    return err;
}

};

codesearch_transport::codesearch_transport(FILE *in, FILE *out) : in_(in), out_(out) {
    assert(!setvbuf(in_,  NULL, _IOFBF, 4096*4));
    assert(!setvbuf(out_, NULL, _IOLBF, 4096));
}

void codesearch_transport::write_frame(const std::string &opcode, json_object *body) {
    json_object *frame = json_frame(opcode, body);
    fprintf(out_, "%s\n", json_object_to_json_string(frame));
    json_object_put(frame);
}

void codesearch_transport::write_match(const match_result *m) {
    json_object *obj = json_object_new_object();

    json_object_object_add(obj, "tree",  to_json(m->file->tree->name));
    json_object_object_add(obj, "version",  to_json(m->file->tree->version));
    json_object_object_add(obj, "path",  to_json(m->file->path));
    json_object_object_add(obj, "lno", to_json(m->lno));
    json_object_object_add(obj, "context_before",
                           to_json(m->context_before));
    json_object_object_add(obj, "context_after",
                           to_json(m->context_after));
    json_object *bounds = json_object_new_array();
    json_object_array_add(bounds, to_json(m->matchleft));
    json_object_array_add(bounds, to_json(m->matchright));
    json_object_object_add(obj, "bounds", bounds);
    json_object_object_add(obj, "line", to_json(m->line));
    write_frame("match", obj);
}

void codesearch_transport::write_error(const std::string &err) {
    write_frame("error", to_json(err));
}

void codesearch_transport::write_ready (const index_info *info) {
    write_frame("ready", to_json(info));
}

void codesearch_transport::write_done(timeval elapsed, const match_stats *stats) {
    write_frame("done", to_json(stats));
}

bool codesearch_transport::read_query(query *q, bool *done) {
    std::string line;
    if (!getline(line, in_)) {
        *done = true;
        return false;
    }
    json_object *js = json_tokener_parse(line.c_str());
    if (is_error(js)) {
        write_error("Parse error: " +
                    string(json_tokener_errors[-(unsigned long)js]));
        return false;
    }

    auto err = parse_object(js, q);
    json_object_put(js);
    if (!err.ok()) {
        write_error(err.err());
    }

    return err.ok();
}

bool parse_index_spec(json_object *in, index_spec *out) {
    json_parse_error err = parse_object(in, "name", &out->name);
    if (!err.ok())
        return false;
    err = parse_object(in, "fs_paths", &out->paths);
    if (!err.ok())
        return false;
    json_object *repos = json_object_object_get(in, "repositories");
    if (repos == NULL)
        return true;
    if (json_object_get_type(repos) == json_type_object) {
        repo_spec s;
        err = parse_object(in, "repositories", &s);
        if (!err.ok())
            return false;
        out->repos.push_back(s);
    } else {
        err = parse_object(in, "repositories", &out->repos);
        if (!err.ok())
            return false;
    }
    return true;
}
