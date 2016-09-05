#include "src/lib/debug.h"

#include "src/codesearch.h"
#include "src/re_width.h"

#include "src/tools/limits.h"
#include "src/tools/grpc_server.h"

#include <json-c/json.h>

#include <string>
#include <algorithm>
#include <functional>

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

using std::string;

string trace_id_from_request(ServerContext *ctx) {
    auto it = ctx->client_metadata().find("request-id");
    if (it == ctx->client_metadata().end())
        return string("");
    return string(it->second.data(), it->second.size());
}

Status CodeSearchImpl::Info(ServerContext* context, const ::InfoRequest* request, ::ServerInfo* response) {
    scoped_trace_id trace(trace_id_from_request(context));
    log("Info()");

    response->set_name(cs_->name());
    std::vector<indexed_tree> trees = cs_->trees();
    for (auto it = trees.begin(); it != trees.end(); ++it) {
        auto insert = response->add_trees();
        insert->set_name(it->name);
        insert->set_version(it->version);
        if (it->metadata == NULL)
            continue;
        auto metadata = insert->mutable_metadata();
        json_object_object_foreach(it->metadata, key, val) {
            switch (json_object_get_type(val)) {
            case json_type_null:
            case json_type_array:
            case json_type_object:
                break;
            case json_type_boolean:
            case json_type_double:
            case json_type_int:
                (*metadata)[string(key)] = string(json_object_to_json_string(val));
                break;
            case json_type_string:
                (*metadata)[string(key)] = string(json_object_get_string(val));
                break;
            }
        }
    }
    response->set_has_tags(ts_ != nullptr);
    return Status::OK;
}

Status extract_regex(std::unique_ptr<RE2> *out,
                     const std::string &label,
                     const std::string &input,
                     RE2::Options &opts) {

    if (input.empty()) {
        out->reset(nullptr);
        return Status::OK;
    }
    std::unique_ptr<RE2> re(new RE2(input, opts));
    if (!re->ok()) {
        return Status(StatusCode::INVALID_ARGUMENT, label + ": " + re->error());
    }
    *out = std::move(re);
    return Status::OK;
}

Status parse_query(query *q, const ::Query* request, ::CodeSearchResult* response) {
    RE2::Options opts;
    default_re2_options(opts);
    opts.set_case_sensitive(!request->fold_case());

    Status status = Status::OK;
    status = extract_regex(&q->line_pat, "line", request->line(), opts);
    if (status.ok())
        status = extract_regex(&q->file_pat, "file", request->file(), opts);
    if (status.ok())
        status = extract_regex(&q->tree_pat, "repo", request->repo(), opts);
    if (status.ok())
        status = extract_regex(&q->tags_pat, "tags", request->tags(), opts);
    if (status.ok())
        status = extract_regex(&q->negate.file_pat, "-file", request->not_file(), opts);
    if (status.ok())
        status = extract_regex(&q->negate.tree_pat, "-repo", request->not_repo(), opts);
    if (status.ok())
        status = extract_regex(&q->negate.tags_pat, "-tags", request->not_tags(), opts);
    return Status::OK;
}

class add_match {
public:
    add_match(CodeSearchResult* response) : response_(response) {}

    void operator()(const match_result *m) const {
        auto result = response_->add_results();
        result->set_tree(m->file->tree->name);
        result->set_version(m->file->tree->version);
        result->set_path(m->file->path);
        result->set_line_number(m->lno);
        std::transform(m->context_before.begin(), m->context_before.end(),
                       RepeatedPtrFieldBackInserter(result->mutable_context_before()),
                       mem_fun_ref(&re2::StringPiece::ToString));

        std::transform(m->context_after.begin(), m->context_after.end(),
                       RepeatedPtrFieldBackInserter(result->mutable_context_after()),
                       mem_fun_ref(&re2::StringPiece::ToString));
        result->mutable_bounds()->set_left(m->matchleft);
        result->mutable_bounds()->set_right(m->matchright);
        result->set_line(m->line.ToString());
    }

private:
    CodeSearchResult* response_;
};

extern long timeval_ms(struct timeval tv);

static std::string pat(const std::unique_ptr<RE2> &p) {
    if (p.get() == 0)
        return "";
    return p->pattern();
}

Status CodeSearchImpl::Search(ServerContext* context, const ::Query* request, ::CodeSearchResult* response) {
    code_searcher::search_thread search(cs_);
    WidthWalker width;

    scoped_trace_id trace(trace_id_from_request(context));

    query q;
    Status st;
    st = parse_query(&q, request, response);
    if (!st.ok())
        return st;

    q.trace_id = current_trace_id();

    log(q.trace_id,
        "processing query line='%s' file='%s' tree='%s' tags='%s' "
        "not_file='%s' not_tree='%s' not_tags='%s'",
        pat(q.line_pat).c_str(),
        pat(q.file_pat).c_str(),
        pat(q.tree_pat).c_str(),
        pat(q.tags_pat).c_str(),
        pat(q.negate.file_pat).c_str(),
        pat(q.negate.tree_pat).c_str(),
        pat(q.negate.tags_pat).c_str());

    if (q.line_pat->ProgramSize() > kMaxProgramSize) {
        log("program too large size=%d", q.line_pat->ProgramSize());
        return Status(StatusCode::INVALID_ARGUMENT, "Parse error");
    }

    int w = width.Walk(q.line_pat->Regexp(), 0);
    if (w > kMaxWidth) {
        log("program too wide width=%d", w);
        return Status(StatusCode::INVALID_ARGUMENT, "Parse error");
    }

    match_stats stats;
    search.match(q, add_match(response), &stats);
    auto out_stats = response->mutable_stats();
    out_stats->set_re2_time(timeval_ms(stats.re2_time));
    out_stats->set_git_time(timeval_ms(stats.git_time));
    out_stats->set_sort_time(timeval_ms(stats.sort_time));
    out_stats->set_index_time(timeval_ms(stats.index_time));
    out_stats->set_analyze_time(timeval_ms(stats.analyze_time));
    switch (stats.why) {
    case kExitNone:
        out_stats->set_exit_reason(SearchStats::NONE);
        break;
    case kExitMatchLimit:
        out_stats->set_exit_reason(SearchStats::MATCH_LIMIT);
        break;
    case kExitTimeout:
        out_stats->set_exit_reason(SearchStats::TIMEOUT);
        break;
    }

    return Status::OK;
}
