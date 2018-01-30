#include "src/codesearch.h"

class scorer {
public:
    scorer(query *q);
    bool operator()(struct match_result *m);

private:
    query *query_;
};

int score_match(const query *q, const match_result* m);
