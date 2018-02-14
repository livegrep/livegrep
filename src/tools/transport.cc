/********************************************************************
 * livegrep -- tools/transport.cc
 * Copyright (c) 2011-2014 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "src/lib/debug.h"
#include "src/lib/timer.h"

#include "src/codesearch.h"
#include "src/tools/transport.h"

#include <json-c/json.h>

#include "gflags/gflags.h"

#include "re2/re2.h"
using re2::RE2;
using std::unique_ptr;

namespace {

json_parse_error parse_object(json_object *j, std::string *);
json_parse_error parse_object(json_object *j, path_spec *);
json_parse_error parse_object(json_object *j, repo_spec *);
json_parse_error parse_object(json_object *j, json_object **);

template <class T>
json_parse_error parse_object(json_object *j, std::vector<T> *);
template<class T>
json_parse_error parse_object(json_object *j, const char *key, T *t);

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

json_parse_error parse_object(json_object *js, json_object **out) {
    if (json_object_get_type(js) != json_type_object)
        return json_parse_error("expected a JSON object");
    *out = json_object_get(js);
    return json_parse_error();
}

json_parse_error parse_object(json_object *js, path_spec *p) {
    if (json_object_get_type(js) != json_type_object)
        return json_parse_error("expected a JSON object");
    json_parse_error err;
    err = parse_object(js, "path", &p->path);
    if (!err.ok()) return err;
    err = parse_object(js, "name", &p->name);
    if (!err.ok()) return err;
    err = parse_object(js, "metadata", &p->metadata);
    if (!err.ok()) return err;
    err = parse_object(js, "ordered-contents", &p->ordered_contents_file_path);
    return err;
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
    if (!err.ok()) return err;
    std::string revision;
    err = parse_object(js, "revision", &revision);
    if (err.ok() && !revision.empty())
        r->revisions.push_back(revision);
    return err;
}

};

json_parse_error parse_index_spec(json_object *in, index_spec *out) {
    json_parse_error err = parse_object(in, "name", &out->name);
    if (!err.ok())
        return err;

    json_object *paths = json_object_object_get(in, "fs_paths");
    if (paths != NULL)
    {
       if (json_object_get_type(paths) == json_type_object) {
           path_spec s;
           err = parse_object(in, "fs_paths", &s);
           if (err.ok())
               out->paths.push_back(s);
       } else {
           err = parse_object(in, "fs_paths", &out->paths);
           if (!err.ok())
               return err;
       }
    }

    json_object *repos = json_object_object_get(in, "repositories");
    if (repos != NULL)
    {
       if (json_object_get_type(repos) == json_type_object) {
           repo_spec s;
           err = parse_object(in, "repositories", &s);
           if (err.ok())
               out->repos.push_back(s);
       } else {
           err = parse_object(in, "repositories", &out->repos);
           if (!err.ok())
               return err;
       }
    }

    return err;
}
