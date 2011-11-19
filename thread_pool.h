#ifndef CODESEARCH_THREAD_POOL_H
#define CODESEARCH_THREAD_POOL_H

#include <pthread.h>

#include "thread_queue.h"

struct no_thread_state;

template <class J, class DoIt, class ThreadState = no_thread_state>
class thread_pool;

template<class J, class DoIt, class ThreadState>
void _thread_worker(thread_pool<J, DoIt, ThreadState>&);
template<class J, class DoIt>
void _thread_worker(thread_pool<J, DoIt, no_thread_state>&);

template <class J, class DoIt, class ThreadState>
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
        _thread_worker(*pool);
        return NULL;
    }

    friend void _thread_worker<>(thread_pool&);
    friend void _thread_worker<>(thread_pool<J, DoIt, no_thread_state>&);
};


struct no_thread_state {};

template<class J, class DoIt, class ThreadState>
void _thread_worker(thread_pool<J, DoIt, ThreadState>& pool)
{
    ThreadState ts(pool.fn_);
    while (true) {
        J job = pool.queue_.pop();
        if (pool.fn_(ts, job))
            break;
    }
}

template <class J, class DoIt>
void _thread_worker(thread_pool<J, DoIt, no_thread_state>& pool)
{
    while (true) {
        J job = pool.queue_.pop();
        if (pool.fn_(job))
            break;
    }
}

#endif /* CODESEARCH_THREAD_POOL_H */

