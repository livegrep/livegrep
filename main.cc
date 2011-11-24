#include "codesearch.h"
#include "smart_git.h"
#include "timer.h"

#include <stdio.h>
#include <iostream>

#include <gflags/gflags.h>

#include <json/json.h>

#include <re2/regexp.h>
#include "re2/walker-inl.h"

DEFINE_bool(json, false, "Use JSON output.");
DEFINE_int32(threads, 4, "Number of threads to use.");
DEFINE_string(dump_index, "", "Dump the produced index to a specified file");
DEFINE_string(load_index, "", "Load the index from a file instead of walking the repository");
DEFINE_string(git_dir, ".git", "The git directory to read from");

using namespace std;
using namespace re2;

long timeval_ms (struct timeval tv) {
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void print_stats(const match_stats &stats) {
    json_object *obj = json_object_new_object();
    json_object_object_add(obj, "re2_time", json_object_new_int
                           (timeval_ms(stats.re2_time)));
    json_object_object_add(obj, "git_time", json_object_new_int
                           (timeval_ms(stats.git_time)));
    json_object_object_add(obj, "sort_time", json_object_new_int
                           (timeval_ms(stats.sort_time)));
    json_object_object_add(obj, "index_time", json_object_new_int
                           (timeval_ms(stats.index_time)));
    printf("DONE %s\n", json_object_to_json_string(obj));
    json_object_put(obj);
}

void print_error(const string& err) {
    if (!FLAGS_json)
        printf("Error: %s\n", err.c_str());
    else
        printf("FATAL %s\n", err.c_str());
}

const int kMaxProgramSize = 4000;
const int kMaxWidth       = 200;

class WidthWalker : public Regexp::Walker<int> {
public:
  virtual int PostVisit(
      Regexp* re, int parent_arg,
      int pre_arg,
      int *child_args, int nchild_args);

  virtual int ShortVisit(
      Regexp* re,
      int parent_arg);
};

int WidthWalker::ShortVisit(Regexp *re, int parent_arg) {
    return 0;
}

int WidthWalker::PostVisit(Regexp *re, int parent_arg,
                           int pre_arg,
                           int *child_args, int nchild_args) {
    int width;
    switch (re->op()) {
    case kRegexpRepeat:
        width = child_args[0] * re->max();
        break;

    case kRegexpNoMatch:
    // These ops match the empty string:
    case kRegexpEmptyMatch:      // anywhere
    case kRegexpBeginLine:       // at beginning of line
    case kRegexpEndLine:         // at end of line
    case kRegexpBeginText:       // at beginning of text
    case kRegexpEndText:         // at end of text
    case kRegexpWordBoundary:    // at word boundary
    case kRegexpNoWordBoundary:  // not at word boundary
        width = 0;
        break;

    case kRegexpLiteral:
    case kRegexpAnyChar:
    case kRegexpAnyByte:
    case kRegexpCharClass:
        width = 1;
        break;

    case kRegexpLiteralString:
        width = re->nrunes();
        break;

    case kRegexpConcat:
        width = 0;
        for (int i = 0; i < nchild_args; i++)
            width += child_args[i];
        break;

    case kRegexpAlternate:
        width = 0;
        for (int i = 0; i < nchild_args; i++)
            width = max(width, child_args[i]);
        break;

    case kRegexpStar:
    case kRegexpPlus:
    case kRegexpQuest:
    case kRegexpCapture:
        width = child_args[0];
        break;

    default:
        assert(false);
    }

    return width;
}

int main(int argc, char **argv) {
    google::SetUsageMessage("Usage: " + string(argv[0]) + " <options> REFS");
    google::ParseCommandLineFlags(&argc, &argv, true);

    git_repository *repo;
    git_repository_open(&repo, FLAGS_git_dir.c_str());

    code_searcher counter(repo);
    counter.set_output_json(FLAGS_json);

    WidthWalker width;

    if (FLAGS_load_index.size() == 0) {
        timer tm;
        struct timeval elapsed;

        for (int i = 1; i < argc; i++) {
            if (!FLAGS_json)
                printf("Walking %s...", argv[i]);
            fflush(stdout);
            counter.walk_ref(argv[i]);
            elapsed = tm.elapsed();
            if (!FLAGS_json)
                printf(" done.\n");
        }
        counter.finalize();
        elapsed = tm.elapsed();
        if (!FLAGS_json)
            printf("repository indexed in %d.%06ds\n",
                   (int)elapsed.tv_sec, (int)elapsed.tv_usec);
    } else {
        counter.load_index(FLAGS_load_index);
    }
    if (!FLAGS_json && !FLAGS_load_index.size())
        counter.dump_stats();
    if (FLAGS_dump_index.size())
        counter.dump_index(FLAGS_dump_index);
    RE2::Options opts;
    opts.set_never_nl(true);
    opts.set_one_line(false);
    opts.set_perl_classes(true);
    opts.set_word_boundary(true);
    opts.set_posix_syntax(true);
    opts.set_log_errors(false);
    while (true) {
        if (FLAGS_json)
            printf("READY\n");
        else
            printf("regex> ");
        string line;
        getline(cin, line);
        if (cin.eof())
            break;
        RE2 re(line, opts);
        if (!re.ok()) {
            print_error(re.error());
            continue;
        }
        if (re.ProgramSize() > kMaxProgramSize) {
            print_error("Parse error.");
            continue;
        }
        int w = width.Walk(re.Regexp(), 0);
        if (w > kMaxWidth) {
            print_error("Parse error.");
            continue;
        }
        {
            timer tm;
            struct timeval elapsed;
            match_stats stats;
            if (!FLAGS_json)
                printf("ProgramSize: %d\n", re.ProgramSize());

            counter.match(re, &stats);
            elapsed = tm.elapsed();
            if (FLAGS_json)
                print_stats(stats);
            else
                printf("Match completed in %d.%06ds.\n",
                       (int)elapsed.tv_sec, (int)elapsed.tv_usec);
        }
    }

    return 0;
}
