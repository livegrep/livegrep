#ifndef CODESEARCH_THREAD_POOL_H
#define CODESEARCH_THREAD_POOL_H

#include <pthread.h>

#include "thread_queue.h"

template <class J, class DoIt>
class thread_pool {
public:
    thread_pool (int nthreads, DoIt& fn)
        : nthreads_(nthreads), fn_(fn) {
        threads_ = new pthread_t[nthreads_];
        for (int i = 0; i < nthreads_; i++) {
            pthread_create(&threads_[i], NULL, worker, this);
        }
    }

    void queue(const J& job) {
        queue_.push(job);
    }

    ~thread_pool () {
        int i;
        for (i = 0; i < nthreads_; i++) {
            pthread_join(threads_[i], NULL);
        }
        delete[] threads_;
    }

protected:
    int nthreads_;
    pthread_t *threads_;
    thread_queue<J> queue_;
    DoIt& fn_;

    static void *worker(void *arg) {
        thread_pool *pool = static_cast<thread_pool*>(arg);
        while (true) {
            J job = pool->queue_.pop();
            if (pool->fn_(job))
                break;
        }
        return NULL;
    }
};


#endif /* CODESEARCH_THREAD_POOL_H */

