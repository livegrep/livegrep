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
#include <mutex>
#include <condition_variable>

template <class T>
class thread_queue {
public:
    thread_queue () : closed_(false) {}

    void push(const T& val) {
        std::unique_lock<std::mutex> locked(mutex_);
        queue_.push_back(val);
        cond_.notify_one();
    }

    void close() {
        std::unique_lock<std::mutex> locked(mutex_);
        closed_ = true;
        cond_.notify_all();
    }

    bool pop(T *out) {
        std::unique_lock<std::mutex> locked(mutex_);
        while (queue_.empty() && !closed_)
            cond_.wait(locked);
        if (queue_.empty() && closed_)
            return false;
        *out = queue_.front();
        queue_.pop_front();
        return true;
    }

    bool try_pop(T *out) {
        std::unique_lock<std::mutex> locked(mutex_);
        if (queue_.empty())
            return false;
        *out = queue_.front();
        queue_.pop_front();
        return true;
    }

 protected:
    thread_queue(const thread_queue&);
    thread_queue operator=(const thread_queue &);
    std::mutex mutex_;
    std::condition_variable cond_;
    bool closed_;
    std::list<T> queue_;
};


#endif /* CODESEARCH_THREAD_QUEUE_H */
