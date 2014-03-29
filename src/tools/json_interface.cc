#include "codesearch.h"
#include "interface.h"
#include "debug.h"
#include "git_indexer.h"
#include "interface-impl.h"

#include <json/json.h>

#include <gflags/gflags.h>

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

static json_object *to_json(const indexed_path &path) {
    json_object *out = json_object_new_object();
    std::string name = path.tree->repo->name + ":" + path.tree->revision;
    json_object_object_add(out, "ref",  to_json(name));
    json_object_object_add(out, "path", to_json(path.path));
    return out;
}

json_object *to_json(const indexed_repo &repo) {
    json_object *out = json_object_new_object();
    json_object_object_add(out, "name", to_json(repo.name));
    if (repo.metadata)
        json_object_object_add(out, "metadata", json_object_get(repo.metadata));
    return out;
}

template <class T>
json_object *to_json(vector<T> vec) {
    json_object *out = json_object_new_array();
    for (auto it = vec.begin(); it != vec.end(); it++)
        json_object_array_add(out, to_json(*it));
    return out;
}

json_object *json_info(const code_searcher *cs) {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "repos", to_json(cs->repos()));
    json_object_object_add(obj, "name", to_json(cs->name()));
    return obj;
}

long timeval_ms (struct timeval tv) {
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

struct repo_spec {
    string path;
    string name;
    vector<string> revisions;
    json_object *metadata;
};

repo_spec parse_repo_spec(json_object *js) {
    debug(kDebugUI, "Parsing: %s", json_object_to_json_string(js));
    if (!json_object_is_type(js, json_type_object)) {
        fprintf(stderr, "Repo spec is not an object!\n");
        exit(1);
    }

    repo_spec spec;
    json_object *js_path = json_object_object_get(js, "path");
    if (json_object_is_type(js_path, json_type_string))
        spec.path = json_object_get_string(js_path);
    json_object *js_name = json_object_object_get(js, "name");
    if (json_object_is_type(js_name, json_type_string))
        spec.name = json_object_get_string(js_name);
    spec.metadata = json_object_get(json_object_object_get(js, "metadata"));

    json_object *js_revs = json_object_object_get(js, "revisions");
    if (json_object_is_type(js_revs, json_type_array)) {
        for (int i = 0; i < json_object_array_length(js_revs); ++i) {
            json_object *elt = json_object_array_get_idx(js_revs, i);
            if (json_object_is_type(elt, json_type_string))
                spec.revisions.push_back(json_object_get_string(elt));
        }
    }

    return spec;
}

void extract_repo_specs(vector<repo_spec> &out, json_object *js) {
    switch(json_object_get_type(js)) {
    case json_type_object:
        debug(kDebugUI, "Parsing a single repo");
        out.push_back(parse_repo_spec(js));
        break;
    case json_type_array:
        debug(kDebugUI, "Parsing an array of repos...");
        for (int i = 0; i < json_object_array_length(js); ++i) {
            out.push_back(parse_repo_spec(json_object_array_get_idx(js, i)));
        }
        break;
    default:
        fprintf(stderr, "Error: unrecognized object type: %s\n",
                json_type_to_name(json_object_get_type(js)));
        exit(1);
    }
}

class json_interface : public codesearch_interface {
public:
    json_interface(FILE *in, FILE *out) : in_(in), out_(out) { }

    virtual void print_match(const match_result *m) {
        json_object *obj = json_object_new_object();
        json_object *contexts = json_object_new_array();
        for (auto ctx = m->context.begin();
             ctx != m->context.end(); ++ctx) {
            json_object *jctx = json_object_new_object();
            json_object_object_add(jctx, "paths",  to_json(ctx->paths));
            json_object_object_add(jctx, "lno", to_json(ctx->lno));
            json_object_object_add(jctx, "context_before",
                                   to_json(ctx->context_before));
            json_object_object_add(jctx, "context_after",
                                   to_json(ctx->context_after));
            json_object_array_add(contexts, jctx);
        }
        json_object_object_add(obj, "contexts", contexts);
        json_object *bounds = json_object_new_array();
        json_object_array_add(bounds, to_json(m->matchleft));
        json_object_array_add(bounds, to_json(m->matchright));
        json_object_object_add(obj, "bounds", bounds);
        json_object_object_add(obj, "line", to_json(m->line));
        fprintf(out_, "%s\n", json_object_to_json_string(obj));
        json_object_put(obj);
    }

    virtual void print_error(const std::string &err) {
        fprintf(out_, "FATAL %s\n", err.c_str());
    }

    virtual void print_prompt(const code_searcher *cs) {
        json_object *info = json_info(cs);
        fprintf(out_, "READY %s\n", json_object_to_json_string(info));
        json_object_put(info);
    }

    virtual bool getline(std::string &input) {
        return ::getline(input, in_);
    }

    virtual bool parse_query(const std::string &input,
                             std::string &line_re,
                             std::string &file_re,
                             std::string &tree_re) {
        json_object *js = json_tokener_parse(input.c_str());
        if (is_error(js)) {
            print_error("Parse error: " +
                        string(json_tokener_errors[-(unsigned long)js]));
            return false;
        }
        if (json_object_get_type(js) != json_type_object) {
            print_error("Expected a JSON object");
            return false;
        }

        json_object *line_js = json_object_object_get(js, "line");
        if (!line_js || json_object_get_type(line_js) != json_type_string) {
            print_error("No regex specified!");
            return false;
        }
        line_re = json_object_get_string(line_js);

        json_object *file_js = json_object_object_get(js, "file");
        if (file_js && json_object_get_type(file_js) == json_type_string)
            file_re = json_object_get_string(file_js);

        json_object *tree_js = json_object_object_get(js, "repo");
        if (tree_js && json_object_get_type(tree_js) == json_type_string)
            tree_re = json_object_get_string(tree_js);

        json_object_put(js);

        return true;
    }

    virtual void print_stats(timeval elapsed, const match_stats *stats) {
        json_object *obj = json_object_new_object();
        json_object_object_add(obj, "re2_time", json_object_new_int
                               (timeval_ms(stats->re2_time)));
        json_object_object_add(obj, "git_time", json_object_new_int
                               (timeval_ms(stats->git_time)));
        json_object_object_add(obj, "sort_time", json_object_new_int
                               (timeval_ms(stats->sort_time)));
        json_object_object_add(obj, "index_time", json_object_new_int
                               (timeval_ms(stats->index_time)));
        json_object_object_add(obj, "analyze_time", json_object_new_int
                               (timeval_ms(stats->analyze_time)));
        switch(stats->why) {
        case kExitNone: break;
        case kExitMatchLimit:
            json_object_object_add(obj, "why", json_object_new_string("limit"));
            break;
        case kExitTimeout:
            json_object_object_add(obj, "why", json_object_new_string("timeout"));
            break;
        }
        fprintf(out_, "DONE %s\n", json_object_to_json_string(obj));
        json_object_put(obj);
    }

    virtual void build_index(code_searcher *cs, const vector<std::string> &argv) {
        if (argv.size() != 2) {
            print_error("Usage: " + argv[0] + " --json [OPTIONS] config.json");
            exit(1);
        }
        json_object *obj = json_object_from_file(argv[1].c_str());
        if (is_error(obj)) {
            print_error(string("Error parsing `") + argv[1] + string("': ") +
                        string(json_tokener_errors[-(unsigned long)obj]));
            exit(1);
        }

        json_object *name = json_object_object_get(obj, "name");
        if (json_object_is_type(name, json_type_string))
            cs->set_name(json_object_get_string(name));

        vector<repo_spec> repos;
        extract_repo_specs(repos, json_object_object_get(obj, "repositories"));
        json_object_put(obj);

        for (auto it = repos.begin(); it != repos.end(); ++it) {
            debug(kDebugUI, "Walking name=%s, path=%s",
                  it->name.c_str(), it->path.c_str());
            git_indexer indexer(cs, it->path, it->name, it->metadata);
            for (auto rev = it->revisions.begin();
                 rev != it->revisions.end(); ++rev) {
                debug(kDebugUI, "  walking %s..", rev->c_str());
                indexer.walk(*rev);
            }
        }
    }

    virtual void info(const char *msg, ...) {}
    virtual ~json_interface() {}

protected:
    FILE *in_, *out_;
};

};

codesearch_interface *make_json_interface(FILE *in, FILE *out) {
    return new json_interface(in, out);
}
