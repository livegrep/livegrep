#include "codesearch.h"
#include "git_indexer.h"
#include "fs_indexer.h"
#include "interface.h"
#include "interface-impl.h"

#include <stdarg.h>
#include <gflags/gflags.h>

DEFINE_bool(filesystem, false, "Analyze a filesystem tree instead of a git repo.");

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

codesearch_interface::~codesearch_interface() {}

namespace {

struct parse_spec {
    string path;
    string name;
    vector<string> revs;
};

parse_spec parse_walk_spec(string spec) {
    /* [name@]path[:rev1,rev2,rev3] */
    parse_spec out;
    int idx;
    if ((idx = spec.find('@')) != -1) {
        out.name = spec.substr(0, idx);
        spec = spec.substr(idx + 1);
    }
    if ((idx = spec.find(':')) != -1) {
        string revs = spec.substr(idx + 1);
        spec = spec.substr(0, idx);
        while ((idx = revs.find(',')) != -1) {
            out.revs.push_back(revs.substr(0, idx));
            revs = revs.substr(idx + 1);
        }
        if (revs.size())
            out.revs.push_back(revs);
    }
    if (out.revs.empty()) {
        out.revs.push_back("HEAD");
    }
    out.path = spec;
    return out;
}

class cli_interface : public codesearch_interface {
public:
    cli_interface(FILE *in, FILE *out) : in_(in), out_(out) { }

    virtual void build_index(code_searcher *cs, const vector<std::string> &argv) {
        if (argv.size() < 2) {
            print_error("Usage: " + argv[0] + " [OPTIONS] <REPOSPEC [...] | -filesystem PATH [...]>");
            exit(1);
        }
        for (auto it = ++argv.begin(); it != argv.end(); ++it) {
            const std::string &arg = *it;
            if (FLAGS_filesystem) {
                this->info("Walking `%s'...\n",
                           arg.c_str());
                fs_indexer indexer(cs, arg.c_str());
                indexer.walk(arg);
                this->info("done\n");
            } else {
                parse_spec parsed = parse_walk_spec(arg);
                this->info("Walking `%s' (name: %s, path: %s)...\n",
                           arg.c_str(),
                           parsed.name.c_str(),
                           parsed.path.c_str());
                git_indexer indexer(cs, parsed.path, parsed.name);
                for (auto it = parsed.revs.begin(); it != parsed.revs.end(); ++it) {
                    this->info("  %s...", it->c_str());
                    indexer.walk(*it);
                    this->info("done\n");
                }
            }
        }
    }

    virtual void print_match(const match_result *m) {
        for (auto ctx = m->context.begin();
             ctx != m->context.end(); ++ctx) {
            for (auto it = ctx->paths.begin(); it != ctx->paths.end(); ++it) {
                fprintf(out_,
                        "%s:%s:%s:%d:%d-%d: %.*s\n",
                        it->tree->repo->name.c_str(),
                        it->tree->revision.c_str(),
                        it->path.c_str(),
                        ctx->lno,
                        m->matchleft, m->matchright,
                        m->line.size(), m->line.data());
            }
        }
    }

    virtual void print_error(const std::string &err) {
        fprintf(out_, "Error: %s\n", err.c_str());
    }

    virtual void print_prompt(const code_searcher *cs) {
        fprintf(out_, "regex> ");
        fflush(out_);
    }

    virtual bool getline(std::string &input) {
        return ::getline(input, in_);
    }

    virtual bool parse_query(const std::string &input,
                             std::string &line,
                             std::string &file,
                             std::string &tree) {
        line = input;
        file.clear();
        tree.clear();
        return true;
    }

    virtual void print_stats(timeval elapsed, const match_stats *stats) {
                fprintf(out_,
                        "Match completed in %d.%06ds.",
                        (int)elapsed.tv_sec, (int)elapsed.tv_usec);
                switch (stats->why) {
                case kExitNone:
                    fprintf(out_, "\n");
                    break;
                case kExitMatchLimit:
                    fprintf(out_, " (match limit)\n");
                    break;
                case kExitTimeout:
                    fprintf(out_, " (timeout)\n");
                    break;
                }
    }

    virtual void info(const char *msg, ...) {
        va_list ap;
        va_start(ap, msg);
        vfprintf(out_, msg, ap);
        va_end(ap);
    }

    virtual ~cli_interface() {}

protected:
    FILE *in_, *out_;
};

};

codesearch_interface *make_cli_interface(FILE *in, FILE *out) {
    return new cli_interface(in, out);
}
