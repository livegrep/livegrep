#include "src/lib/debug.h"
#include "src/lib/timer.h"

#include "src/codesearch.h"
#include "src/tagsearch.h"
#include "src/re_width.h"

#include "src/tools/limits.h"
#include "src/tools/grpc_server.h"

#include "google/protobuf/repeated_field.h"

#include "gflags/gflags.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <future>
#include <string>

#include "utf8.h"

#include <boost/bind.hpp>

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

using std::string;

DEFINE_int32(context_lines, 3, "The default number of result context lines to provide for a single query.");
DEFINE_int32(max_matches, 50, "The default maximum number of matches to return for a single query.");

class CodeSearchImpl final : public CodeSearch::Service {
 public:
    explicit CodeSearchImpl(code_searcher *cs, code_searcher *tagdata, std::promise<void> *reload_request);
    virtual ~CodeSearchImpl();

    virtual grpc::Status Info(grpc::ServerContext* context, const ::InfoRequest* request, ::ServerInfo* response);
    void TagsFirstSearch_(::CodeSearchResult* response, query& q, match_stats& stats);
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
        insert->mutable_metadata()->CopyFrom(it->metadata);
    }
    response->set_has_tags(tagdata_ != nullptr);
    response->set_index_time(cs_->index_timestamp());
    return Status::OK;
}

Status extract_regex(std::shared_ptr<RE2> *out,
                     const std::string &label,
                     const std::string &input,
                     bool case_sensitive) {
    if (input.empty()) {
        out->reset();
        return Status::OK;
    }

    RE2::Options opts;
    default_re2_options(opts);
    opts.set_case_sensitive(case_sensitive);

    std::shared_ptr<RE2> re(new RE2(input, opts));
    if (!re->ok()) {
        return Status(StatusCode::INVALID_ARGUMENT, label + ": " + re->error());
    }
    *out = std::move(re);
    return Status::OK;
}

Status extract_regex(std::shared_ptr<RE2> *out,
                     const std::string &label,
                     const std::string &input) {
    if (input.empty()) {
        out->reset();
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
    q->context_lines = request->context_lines();
    if (q->context_lines <= 0 && FLAGS_context_lines) {
        q->context_lines = FLAGS_context_lines;
    }
    return status;
}

class add_match {
    void insert_string_back(google::protobuf::RepeatedPtrField<string> *field, StringPiece str) const {
        if (utf8::is_valid(str.begin(), str.end())) {
            field->Add(str.ToString());
        } else {
            field->Add("<invalid utf-8>");
        }
    }

public:
    typedef std::set<std::pair<indexed_file*, int>> line_set;

    add_match(line_set* ls, CodeSearchResult* response)
        : unique_lines_(ls), response_(response) {}

    int match_count() {
        return response_->results_size();
    }

    void operator()(const match_result *m) const {
        // Avoid a duplicate if a line is returned once from the
        // tags search then again during the main corpus search.
        bool already_inserted = ! unique_lines_->insert(std::make_pair(m->file, m->lno)).second;
        if (already_inserted) {
            return;
        }

        auto result = response_->add_results();
        result->set_tree(m->file->tree->name);
        result->set_version(m->file->tree->version);
        result->set_path(m->file->path);
        result->set_line_number(m->lno);
        for (auto &piece : m->context_before) {
            insert_string_back(result->mutable_context_before(), piece);
        }
        for (auto &piece : m->context_after) {
            insert_string_back(result->mutable_context_after(), piece);
        }
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
    line_set* unique_lines_;
    CodeSearchResult* response_;
};

static void run_tags_search(const query& main_query, std::string regex,
                            code_searcher *tagdata, add_match& cb,
                            tag_searcher* searcher, match_stats& stats) {
    // copy of the main query that we will edit into a query of the tags
    // file for the pattern `regex`
    query q = main_query;
    q.line_pat.reset(new RE2(regex, q.line_pat->options()));

    // the negation constraints will be checked when we transform the match
    // (unfortunately, we can't construct a line query that checks these)
    query constraints;
    constraints.line_pat = main_query.line_pat;  // tell it what to highlight
    constraints.negate.file_pat.swap(q.negate.file_pat);
    constraints.negate.tags_pat.swap(q.negate.tags_pat);

    // modify the line pattern to match the constraints that we can handle now
    regex = tag_searcher::create_tag_line_regex_from_query(&q);
    q.line_pat.reset(new RE2(regex, q.line_pat->options()));
    q.file_pat.reset();
    q.tags_pat.reset();

    code_searcher::search_thread search(tagdata);
    search.match(q,
                 cb,
                 cb,
                 boost::bind(&tag_searcher::transform, searcher, &constraints, _1),
                 &stats);
}

static std::string pat(const std::shared_ptr<RE2> &p) {
    if (p.get() == 0)
        return "";
    return p->pattern();
}

void CodeSearchImpl::TagsFirstSearch_(::CodeSearchResult* response, query& q, match_stats& stats) {
    string line_pat = q.line_pat->pattern();
    string regex;
    int32_t original_max_matches = q.max_matches;  // remember original value

    add_match::line_set ls;
    add_match cb(&ls, response);

    /* To surface the most important matches first, start with tags.
       First pass: is the pattern an exact match for any tags? */
    regex = "^" + line_pat + "$";
    run_tags_search(q, regex, tagdata_, cb, tagmatch_, stats);

    q.max_matches = original_max_matches - cb.match_count();
    if (q.max_matches <= 0)
        return;

    /* Second pass: is the pattern a prefix match for any tags? */
    regex = "^" + line_pat + "[^\t]";
    run_tags_search(q, regex, tagdata_, cb, tagmatch_, stats);

    q.max_matches = original_max_matches - cb.match_count();
    if (q.max_matches <= 0)
        return;

    /* Third and final pass: full corpus search. */
    code_searcher::search_thread *search;
    if (!pool_.try_pop(&search))
        search = new code_searcher::search_thread(cs_);
    search->match(q, cb, cb, &stats);
    pool_.push(search);
}

Status CodeSearchImpl::Search(ServerContext* context, const ::Query* request, ::CodeSearchResult* response) {
    WidthWalker width;

    scoped_trace_id trace(trace_id_from_request(context));

    response->set_index_name(cs_->name());
    response->set_index_time(cs_->index_timestamp());

    query q;
    Status st;
    st = parse_query(&q, request, response);
    if (!st.ok())
        return st;

    q.trace_id = current_trace_id();

    q.max_matches = request->max_matches();
    if (q.max_matches == 0 && FLAGS_max_matches) {
        // For zero-valued match limits, defer to the command line-specified default
        q.max_matches = FLAGS_max_matches;
    } else if (q.max_matches < 0) {
        // For explicitly negative match limits, disable the match limiter entirely
        q.max_matches = 0;
    }

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

    string line_pat = q.line_pat->pattern();

    /* Patterns like "User.*Info" and "p\d+" might be genuine attempts
       to match tags, so we cannot too quickly dismiss odd-looking REs
       as justifying our skipping the phases of a tags search.  But we
       can at least special-case a few characters that would not ever
       appear in a pattern that matches tags.
       TODO(brandon-rhodes): make this more sophisticated. */
    bool might_match_tags =
        // Characters that can't appear in an RE that matches a tag.
        line_pat.find_first_of(" !\"#%&',-/;<=>@`") == string::npos
        // If the user anchored the RE, then we can only run it against
        // whole lines from the corpus, never against tags.
        && line_pat.front() != '^'
        && line_pat.back() != '$'
        ;

    match_stats stats;
    timer search_tm(true);
    if (q.tags_pat == NULL && tagdata_ && might_match_tags) {
        CodeSearchImpl::TagsFirstSearch_(response, q, stats);
    } else if (q.tags_pat == NULL) {
        code_searcher::search_thread *search;
        if (!pool_.try_pop(&search))
            search = new code_searcher::search_thread(cs_);
        add_match::line_set ls;
        add_match cb(&ls, response);
        search->match(q, cb, cb, &stats);
        pool_.push(search);
    } else {
        if (tagdata_ == NULL)
            return Status(StatusCode::FAILED_PRECONDITION, "No tags file available.");

        add_match::line_set ls;
        add_match cb(&ls, response);
        run_tags_search(q, line_pat, tagdata_, cb, tagmatch_, stats);
    }
    search_tm.pause();

    auto out_stats = response->mutable_stats();
    out_stats->set_re2_time(timeval_ms(stats.re2_time));
    out_stats->set_git_time(timeval_ms(stats.git_time));
    out_stats->set_sort_time(timeval_ms(stats.sort_time));
    out_stats->set_index_time(timeval_ms(stats.index_time));
    out_stats->set_analyze_time(timeval_ms(stats.analyze_time));
    out_stats->set_total_time(timeval_ms(search_tm.elapsed()));
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
