#ifndef CODESEARCH_THREAD_QUEUE_H
#define CODESEARCH_THREAD_QUEUE_H

#include <list>

#include "mutex.h"

template <class T>
class thread_queue {
public:
    thread_queue () {}

    void push(const T& val) {
        mutex_locker locked(mutex_);
        queue_.push_back(val);
        cond_.signal();
    }

    T pop() {
        mutex_locker locked(mutex_);
        while (queue_.empty())
            cond_.wait(&mutex_);
        T rv = queue_.front();
        queue_.pop_front();
        return rv;
    }

    bool try_pop(T &ret) {
        mutex_locker locked(mutex_);
        if (queue_.empty())
            return false;
        ret = queue_.front();
        queue_.pop_front();
        return true;
    }

 protected:
    thread_queue(const thread_queue&);
    thread_queue operator=(const thread_queue &);
    cs_mutex mutex_;
    cond_var cond_;
    std::list<T> queue_;
};


#endif /* CODESEARCH_THREAD_QUEUE_H */
