#include "codesearch.h"
#include "interface.h"
#include "interface-impl.h"

#include <json/json.h>

#include <gflags/gflags.h>

DECLARE_string(name);

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
    json_object_object_add(out, "ref",  to_json(path.tree->name));
    json_object_object_add(out, "path", to_json(path.path));
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
    json_object_object_add(obj, "name", to_json(FLAGS_name));
    json_object_object_add(obj, "trees", to_json(cs->tree_names()));
    return obj;
}

long timeval_ms (struct timeval tv) {
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

class json_interface : public codesearch_interface {
public:
    json_interface(FILE *in, FILE *out) : in_(in), out_(out) { }

    virtual void print_match(const match_result *m) {
        json_object *obj = json_object_new_object();
        json_object_object_add(obj, "ref",
                               to_json(m->context[0].paths[0].tree->name));
        json_object_object_add(obj, "file",
                               to_json(m->context[0].paths[0].path));
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
        json_object_object_add(obj, "lno",  json_object_new_int(m->context[0].lno));
        json_object *bounds = json_object_new_array();
        json_object_array_add(bounds, to_json(m->matchleft));
        json_object_array_add(bounds, to_json(m->matchright));
        json_object_object_add(obj, "bounds", bounds);
        json_object_object_add(obj, "line", to_json(m->line));
        json_object_object_add(obj, "context_before",
                               to_json(m->context[0].context_before));
        json_object_object_add(obj, "context_after",
                               to_json(m->context[0].context_after));
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

    virtual void info(const char *msg, ...) {}
    virtual ~json_interface() {}

protected:
    FILE *in_, *out_;
};

};

codesearch_interface *make_json_interface(FILE *in, FILE *out) {
    return new json_interface(in, out);
}
