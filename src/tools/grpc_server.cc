#include "src/lib/debug.h"
#include "src/lib/timer.h"

#include "src/codesearch.h"
#include "src/tagsearch.h"
#include "src/re_width.h"
#include "src/score.h"

#include "src/tools/limits.h"
#include "src/tools/grpc_server.h"

#include <json-c/json.h>

#include <algorithm>
#include <cctype>
#include <functional>
#include <future>
#include <string>

#include <boost/bind.hpp>

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

using std::string;

class CodeSearchImpl final : public CodeSearch::Service {
 public:
    explicit CodeSearchImpl(code_searcher *cs, code_searcher *tagdata, std::promise<void> *reload_request);
    virtual ~CodeSearchImpl();

    virtual grpc::Status Info(grpc::ServerContext* context, const ::InfoRequest* request, ::ServerInfo* response);
    virtual grpc::Status Search(grpc::ServerContext* context, const ::Query* request, ::CodeSearchResult* response);
    virtual grpc::Status Reload(grpc::ServerContext* context, const ::Empty* request, ::Empty* response);

 private:
    code_searcher *cs_;
    code_searcher *tagdata_;
    std::promise<void> *reload_request_;
    tag_searcher *tagmatch_;

    thread_queue <code_searcher::search_thread*> pool_;
};

std::unique_ptr<CodeSearch::Service> build_grpc_server(code_searcher *cs,
                                                       code_searcher *tagdata,
                                                       std::promise<void> *reload_request) {
    return std::unique_ptr<CodeSearch::Service>(new CodeSearchImpl(cs, tagdata, reload_request));
}

CodeSearchImpl::CodeSearchImpl(code_searcher *cs, code_searcher *tagdata, std::promise<void> *reload_request)
    : cs_(cs), tagdata_(tagdata), reload_request_(reload_request), tagmatch_(nullptr) {
    if (tagdata != nullptr) {
        tagmatch_ = new tag_searcher;
        tagmatch_->cache_indexed_files(cs_);
    }
}

CodeSearchImpl::~CodeSearchImpl() {
    pool_.close();
    code_searcher::search_thread* thread;
    while (pool_.pop(&thread))
        delete thread;
    delete tagmatch_;
}

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
    response->set_has_tags(tagdata_ != nullptr);
    response->set_index_time(cs_->index_timestamp());
    return Status::OK;
}

Status extract_regex(std::unique_ptr<RE2> *out,
                     const std::string &label,
                     const std::string &input,
                     bool case_sensitive) {
    if (input.empty()) {
        out->reset(nullptr);
        return Status::OK;
    }

    RE2::Options opts;
    default_re2_options(opts);
    opts.set_case_sensitive(case_sensitive);

    std::unique_ptr<RE2> re(new RE2(input, opts));
    if (!re->ok()) {
        return Status(StatusCode::INVALID_ARGUMENT, label + ": " + re->error());
    }
    *out = std::move(re);
    return Status::OK;
}

Status extract_regex(std::unique_ptr<RE2> *out,
                     const std::string &label,
                     const std::string &input) {
    if (input.empty()) {
        out->reset(nullptr);
        return Status::OK;
    }
    bool case_sensitive = std::any_of(input.begin(), input.end(), isupper);
    return extract_regex(out, label, input, case_sensitive);
}

Status parse_query(query *q, const ::Query* request, ::CodeSearchResult* response) {
    Status status = Status::OK;
    status = extract_regex(&q->line_pat, "line", request->line(), !request->fold_case());
    if (status.ok())
        status = extract_regex(&q->file_pat, "file", request->file());
    if (status.ok())
        status = extract_regex(&q->tree_pat, "repo", request->repo());
    if (status.ok())
        status = extract_regex(&q->tags_pat, "tags", request->tags());
    if (status.ok())
        status = extract_regex(&q->negate.file_pat, "-file", request->not_file());
    if (status.ok())
        status = extract_regex(&q->negate.tree_pat, "-repo", request->not_repo());
    if (status.ok())
        status = extract_regex(&q->negate.tags_pat, "-tags", request->not_tags());
    q->filename_only = request->filename_only();
    return status;
}

class collect_match {
public:
    collect_match(std::set<std::pair<std::pair<int, string>, const match_result*>> *ordered_matches) : matches_(ordered_matches) {}

    void operator()(const match_result *m) {
        // Sort matches by score, then repo:filename.
        auto sort_key = std::make_pair(-m->score, m->file->tree->name + ":" + m->file->path);
        matches_->insert(std::make_pair(sort_key, new match_result(*m)));
    }

private:
    std::set<std::pair<std::pair<int, string>, const match_result*>> *matches_;
};

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

    void operator()(const file_result *f) const {
        auto result = response_->add_file_results();
        result->set_tree(f->file->tree->name);
        result->set_version(f->file->tree->version);
        result->set_path(f->file->path);
        result->mutable_bounds()->set_left(f->matchleft);
        result->mutable_bounds()->set_right(f->matchright);
    }

private:
    CodeSearchResult* response_;
};

static std::string pat(const std::unique_ptr<RE2> &p) {
    if (p.get() == 0)
        return "";
    return p->pattern();
}

Status CodeSearchImpl::Search(ServerContext* context, const ::Query* request, ::CodeSearchResult* response) {
    WidthWalker width;

    scoped_trace_id trace(trace_id_from_request(context));

    query q;
    Status st;
    st = parse_query(&q, request, response);
    if (!st.ok())
        return st;

    q.trace_id = current_trace_id();
    q.max_matches = request->max_matches();

    log(q.trace_id,
        "processing query line='%s' file='%s' tree='%s' tags='%s' "
        "not_file='%s' not_tree='%s' not_tags='%s' max_matches='%d'",
        pat(q.line_pat).c_str(),
        pat(q.file_pat).c_str(),
        pat(q.tree_pat).c_str(),
        pat(q.tags_pat).c_str(),
        pat(q.negate.file_pat).c_str(),
        pat(q.negate.tree_pat).c_str(),
        pat(q.negate.tags_pat).c_str(),
        q.max_matches);

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

    add_match add_cb(response);
    std::set<std::pair<std::pair<int, string>, const match_result*>> ordered_matches;
    collect_match collect_cb(&ordered_matches);

    if (q.tags_pat == NULL) {
        code_searcher::search_thread *search;
        if (!pool_.try_pop(&search))
            search = new code_searcher::search_thread(cs_);

        scorer score(&q);

        search->match(q, collect_cb, add_cb, score, &stats);

        pool_.push(search);
    } else {
        if (tagdata_ == NULL)
            return Status(StatusCode::FAILED_PRECONDITION, "No tags file available.");

        code_searcher::search_thread search(tagdata_);

        // the negation constraints will be checked when we transform the match
        // (unfortunately, we can't construct a line query that checks these)
        query constraints;
        constraints.negate.file_pat.swap(q.negate.file_pat);
        constraints.negate.tags_pat.swap(q.negate.tags_pat);

        // modify the line pattern to match the constraints that we can handle now
        std::string regex = tag_searcher::create_tag_line_regex_from_query(&q);
        q.line_pat.reset(new RE2(regex, q.line_pat->options()));
        q.file_pat.reset();
        q.tags_pat.reset();

        search.match(q,
                     collect_cb,
                     add_cb,
                     boost::bind(&tag_searcher::transform, tagmatch_, &constraints, _1),
                     &stats);
    }

    for (auto it = ordered_matches.begin(); it != ordered_matches.end(); it++) {
        add_cb(it->second);
        delete it->second;
    }

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

Status CodeSearchImpl::Reload(ServerContext* context, const ::Empty* request, ::Empty* response) {
    log("Reload()");
    if (reload_request_ == NULL) {
      return Status(StatusCode::UNIMPLEMENTED, "reload rpc not enabled");
    }
    reload_request_->set_value();
    return Status::OK;
}
