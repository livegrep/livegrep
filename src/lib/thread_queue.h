/********************************************************************
 * livegrep -- thread_queue.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
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

    void close() {
        mutex_locker locked(mutex_);
        closed_ = true;
        cond_.broadcast();
    }

    bool pop(T *out) {
        mutex_locker locked(mutex_);
        while (queue_.empty() && !closed_)
            cond_.wait(&mutex_);
        if (queue_.empty() && closed_)
            return false;
        *out = queue_.front();
        queue_.pop_front();
        return true;
    }
 protected:
    thread_queue(const thread_queue&);
    thread_queue operator=(const thread_queue &);
    cs_mutex mutex_;
    cond_var cond_;
    bool closed_;
    std::list<T> queue_;
};


#endif /* CODESEARCH_THREAD_QUEUE_H */
