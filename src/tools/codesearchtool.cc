#include <gflags/gflags.h>
#include <stdio.h>

#include <string>

using std::string;

extern int analyze_re(int, char**);
extern int dump_file(int, char**);
extern int inspect_index(int, char**);

struct _command {
    string name;
    int (*fn)(int, char**);
} commands[] = {
    {"analyze-re", analyze_re},
    {"inspect-index", inspect_index},
    {"dump-file", dump_file},
};

int main(int argc, char **argv) {
    gflags::SetUsageMessage("Usage: " + string(argv[0]) + " <options> COMMAND ARGS");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    string me(argv[0]);
    size_t i = me.rfind('/');
    if (i != string::npos)
        me = me.substr(i+1);

    for (auto it = &(commands[0]);
         it != &(commands[0]) + sizeof(commands)/sizeof(commands[0]);
         ++it) {
        if (it->name == me)
            return it->fn(argc-1, argv+1);
    }

    fprintf(stderr, "Unknown tool: %s\n", me.c_str());

    return 1;
}
