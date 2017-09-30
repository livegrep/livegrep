#include "src/lib/debug.h"
#include "src/score.h"

scorer::scorer(query *q) : query_(q) {}

bool scorer::operator()(match_result *m) {
    int score = score_match(query_, m);
    m->score = score;
    return true;
}

int score_match(const query *q, const match_result *m) {
    // Simple scoring implementation based purely on query and filepath.
    int score = 0;
    if (m->file->path.find("generated") != string::npos ||
            m->file->path.find("_pb2") != string::npos ||
            m->file->path.find(".min.") != string::npos ||
            m->file->path.find("/minified/") != string::npos ||
            m->file->path.find("bundle") != string::npos) {
        score -= 50;
    }
    if (m->file->path.find("/third_party/") != string::npos ||
            m->file->path.find("/thirdparty/") != string::npos ||
            m->file->path.find("/vendor/") != string::npos ||
            m->file->path.find("/github.com/") != string::npos ||
            m->file->path.find("/node_modules/") != string::npos) {
        score -= 50;
    }
    if (m->file->path.find("test") != string::npos &&
            !((q->line_pat && q->line_pat->pattern().find("test") != string::npos) ||
              (q->file_pat && q->file_pat->pattern().find("test") != string::npos))) {
        score -= 20;
    }
    if (q->line_pat && m->file->path.find(q->line_pat->pattern()) != string::npos) {
        score += 200;
    }
    return score;
}
