#ifndef CODESEARCH_GRPC_SERVER_H
#define CODESEARCH_GRPC_SERVER_H

#include "src/proto/livegrep.grpc.pb.h"

class code_searcher;
class tag_searcher;

class CodeSearchImpl final : public CodeSearch::Service {
 public:
    explicit CodeSearchImpl(code_searcher *cs, code_searcher *tagdata);
    virtual ~CodeSearchImpl();

    virtual grpc::Status Info(grpc::ServerContext* context, const ::InfoRequest* request, ::ServerInfo* response);
    virtual grpc::Status Search(grpc::ServerContext* context, const ::Query* request, ::CodeSearchResult* response);

 private:
    code_searcher *cs_;
    code_searcher *tagdata_;
    tag_searcher *tagmatch_;
};

#endif /* CODESEARCH_GRPC_SERVER_H */
