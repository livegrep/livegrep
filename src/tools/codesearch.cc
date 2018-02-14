/********************************************************************
 * livegrep -- tools/codesearch.cc
 * Copyright (c) 2011-2014 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "src/lib/timer.h"
#include "src/lib/metrics.h"
#include "src/lib/debug.h"

#include "src/codesearch.h"
#include "src/tagsearch.h"
#include "src/re_width.h"
#include "src/git_indexer.h"
#include "src/fs_indexer.h"

#include "src/tools/transport.h"
#include "src/tools/limits.h"
#include "src/tools/grpc_server.h"

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <semaphore.h>

#include <iostream>
#include <functional>
#include <future>
#include <thread>
#include <memory>

#include <gflags/gflags.h>

#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include "re2/regexp.h"
#include "re2/walker-inl.h"

#include <json-c/json.h>

#include <grpc++/server.h>
#include <grpc++/server_builder.h>

DEFINE_string(dump_index, "", "Dump the produced index to a specified file");
DEFINE_string(load_index, "", "Load the index from a file instead of walking the repository");
DEFINE_string(load_tags, "", "Load the index built from a tags file.");
DEFINE_bool(quiet, false, "Do the search, but don't print results.");
DEFINE_bool(index_only, false, "Build the index and don't serve queries");
DEFINE_string(grpc, "localhost:9999", "GRPC listener address");
DEFINE_bool(reload_rpc, false, "Enable the Reload RPC");

using namespace std;
using namespace re2;
namespace fs = boost::filesystem;

using grpc::Server;
using grpc::ServerBuilder;

void die_errno(const char *str) {
    perror(str);
    exit(1);
}

void build_index(code_searcher *cs, const vector<std::string> &argv) {
    if (argv.size() != 2) {
        fprintf(stderr, "Usage: %s [OPTIONS] config.json\n", argv[0].c_str());
        exit(1);
    }

    fs::path config_file_path(argv[1]);
    json_object *obj = json_object_from_file(config_file_path.c_str());
    if (is_error(obj)) {
        fprintf(stderr, "Error parsing `%s'\n",
                argv[1].c_str());
        exit(1);
    }
    index_spec spec;
    json_parse_error err = parse_index_spec(obj, &spec);
    if (!err.ok()) {
        fprintf(stderr, "parsing %s: %s\n", argv[1].c_str(), err.err().c_str());
        exit(1);
    }
    if (spec.paths.empty() && spec.repos.empty()) {
        fprintf(stderr, "%s: You must specify at least one path to index.\n", argv[1].c_str());
        exit(1);
    }
    json_object_put(obj);
    if (spec.name.size())
        cs->set_name(spec.name);
    for (auto it = spec.paths.begin(); it != spec.paths.end(); ++it) {
        fprintf(stderr, "Walking path_spec name=%s, path=%s\n",
                it->name.c_str(), it->path.c_str());
        fs_indexer indexer(cs, it->path, it->name, it->metadata);
        if (it->ordered_contents_file_path.empty()) {
            fprintf(stderr, "  walking full tree\n");
            indexer.walk(it->path);
        } else {
            fprintf(stderr, "  walking paths from ordered contents list\n");
            fs::path contents_file_path = fs::canonical(it->ordered_contents_file_path, config_file_path.remove_filename());
            indexer.walk_contents_file(contents_file_path);
        }
        fprintf(stderr, "done\n");
    }

    for (auto it = spec.repos.begin(); it != spec.repos.end(); ++it) {
        fprintf(stderr, "Walking repo_spec name=%s, path=%s\n",
                it->name.c_str(), it->path.c_str());
        git_indexer indexer(cs, it->path, it->name, it->metadata);
        for (auto rev = it->revisions.begin();
             rev != it->revisions.end(); ++rev) {
            fprintf(stderr, "  walking %s... ", rev->c_str());
            indexer.walk(*rev);
            fprintf(stderr, "done\n");
        }
    }
}

void initialize_search(code_searcher *search,
                       int argc, char **argv) {
    if (FLAGS_load_index.size() == 0) {
        if (FLAGS_dump_index.size())
            search->set_alloc(make_dump_allocator(search, FLAGS_dump_index));
        else
            search->set_alloc(make_mem_allocator());

        vector<std::string> args;
        for (int i = 0; i < argc; ++i)
            args.push_back(argv[i]);

        timer tm;
        struct timeval elapsed;
        build_index(search, args);
        fprintf(stderr, "Finalizing...\n");
        search->finalize();
        elapsed = tm.elapsed();
        fprintf(stderr, "repository indexed in %d.%06ds\n",
                (int)elapsed.tv_sec, (int)elapsed.tv_usec);
        metric::dump_all();
    } else {
        search->load_index(FLAGS_load_index);
    }
    if (FLAGS_dump_index.size() && FLAGS_load_index.size())
        search->dump_index(FLAGS_dump_index);
}

void listen_grpc(code_searcher *search, code_searcher *tags, const string& addr) {
    promise<void> reload_request;
    auto reload_request_ptr = FLAGS_reload_rpc ? &reload_request : NULL;

    unique_ptr<CodeSearch::Service> service(build_grpc_server(search, tags, reload_request_ptr));

    ServerBuilder builder;
    builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
    builder.RegisterService(service.get());
    std::unique_ptr<Server> server(builder.BuildAndStart());

    log("Serving...");

    if (FLAGS_reload_rpc) {
        thread shutdown_thread([&]() {
            reload_request.get_future().wait();
            server->Shutdown();
        });
        server->Wait();
        shutdown_thread.join();
    } else {
        server->Wait();
    }
}

int main(int argc, char **argv) {
    gflags::SetUsageMessage("Usage: " + string(argv[0]) + " <options> REFS");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    signal(SIGPIPE, SIG_IGN);

    while (true) {
        code_searcher search;
        unique_ptr<code_searcher> tags;

        initialize_search(&search, argc, argv);
        if (FLAGS_load_tags.size() != 0) {
            tags.reset(new code_searcher());
            tags->load_index(FLAGS_load_tags);
        }

        if (FLAGS_index_only)
            return 0;

        if (FLAGS_grpc.size()) {
            listen_grpc(&search, tags.get(), FLAGS_grpc);
        }
    }
}
