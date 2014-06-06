#include "metrics.h"
#include "mutex.h"

#include <stdlib.h>
#include <map>

namespace {
    cs_mutex metrics_mtx;
    std::map<std::string, metric*> *metrics;
};


metric::metric(const std::string &name) {
    mutex_locker locked(metrics_mtx);
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
