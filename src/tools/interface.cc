#include "codesearch.h"
#include "interface.h"
#include "interface-impl.h"

#include <stdarg.h>

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

class cli_interface : public codesearch_interface {
public:
    cli_interface(FILE *in, FILE *out) : in_(in), out_(out) { }
    virtual void print_match(const match_result *m) {
        for (auto ctx = m->context.begin();
             ctx != m->context.end(); ++ctx) {
            for (auto it = ctx->paths.begin(); it != ctx->paths.end(); ++it) {
                fprintf(out_,
                        "%s:%s:%d:%d-%d: %.*s\n",
                        it->tree->name.c_str(),
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
