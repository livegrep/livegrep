#ifndef CODESEARCH_GRPC_SERVER_H
#define CODESEARCH_GRPC_SERVER_H

#include "src/proto/livegrep.grpc.pb.h"
#include <memory>

class code_searcher;
class tag_searcher;

std::unique_ptr<CodeSearch::Service> build_grpc_server(code_searcher *cs, code_searcher *tagdata);

#endif /* CODESEARCH_GRPC_SERVER_H */
