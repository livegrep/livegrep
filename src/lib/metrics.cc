#include "metrics.h"

#include <stdlib.h>
#include <map>
#include <mutex>

namespace {
    std::mutex metrics_mtx;
    std::map<std::string, metric*> *metrics;
};


metric::metric(const std::string &name) {
    std::unique_lock<std::mutex> locked(metrics_mtx);
    if (metrics == 0)
        metrics = new std::map<std::string, metric*>;
    (*metrics)[name] = this;
}


void metric::dump_all() {
    fprintf(stderr, "== begin metrics ==\n");
    for (auto it = metrics->begin(); it != metrics->end(); ++it) {
        fprintf(stderr, "%s %ld\n", it->first.c_str(), it->second->val_.load());
    }
    fprintf(stderr, "== end metrics ==\n");
}
