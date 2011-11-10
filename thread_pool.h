#ifndef CODESEARCH_THREAD_POOL_H
#define CODESEARCH_THREAD_POOL_H

#include <pthread.h>

#include "thread_queue.h"

template <class J, class DoIt>
class base_thread_pool {
public:
    base_thread_pool (int nthreads, DoIt& fn)
        : nthreads_(nthreads), fn_(fn) {
        threads_ = new pthread_t[nthreads_];
    }

    void queue(const J& job) {
        queue_.push(job);
    }

    virtual ~base_thread_pool () {
        int i;
        for (i = 0; i < nthreads_; i++) {
            pthread_join(threads_[i], NULL);
        }
        delete threads_;
    }

protected:
    int nthreads_;
    pthread_t *threads_;
    thread_queue<J> queue_;
    DoIt& fn_;

    virtual void worker()=0;

    static void *worker(void *arg) {
        base_thread_pool *pool = static_cast<base_thread_pool*>(arg);
        pool->worker();
        return NULL;
    }

    void start_threads() {
        for (int i = 0; i < nthreads_; i++) {
            pthread_create(&threads_[i], NULL, worker, this);
        }
    }
};

struct no_thread_state {};

template<class J, class DoIt, class ThreadState = no_thread_state>
class thread_pool;

template<class J, class DoIt, class ThreadState>
class thread_pool : public base_thread_pool <J, DoIt> {
public:
    thread_pool(int nthreads, DoIt& doit)
        : base_thread_pool<J, DoIt>(nthreads, doit) {
        this->start_threads();
    }

    virtual ~thread_pool() {}

protected:
    virtual void worker()
    {
        ThreadState ts(this->fn_);
        while (true) {
            J job = this->queue_.pop();
            if (this->fn_(ts, job))
                break;
        }
    }
};

template <class J, class DoIt>
class thread_pool<J, DoIt, no_thread_state> :
    public base_thread_pool <J, DoIt> {
public:
    thread_pool(int nthreads, DoIt& doit)
        : base_thread_pool<J, DoIt>(nthreads, doit) {
        this->start_threads();
    }

    virtual ~thread_pool() {}

protected:
    virtual void worker()
    {
        while (true) {
            J job = this->queue_.pop();
            if (fn_(job))
                break;
        }
    }
};

#endif /* CODESEARCH_THREAD_POOL_H */

