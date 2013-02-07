/********************************************************************
 * livegrep -- per_thread.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_PER_THREAD_H
#define CODESEARCH_PER_THREAD_H

#include <pthread.h>

template <class T>
class per_thread {
public:
    per_thread() {
        pthread_key_create(&key_, destroy);
    }
    ~per_thread() {
        pthread_key_delete(key_);
    }

    T *get() const {
        return static_cast<T*>(pthread_getspecific(key_));
    }

    T *put(T *obj) {
        T *old = get();
        pthread_setspecific(key_, obj);
        return old;
    }

    T *operator->() const {
        return get();
    }

    T& operator*() const {
        return *get();
    }
private:

    static void destroy(void *obj) {
        T *t = static_cast<T*>(obj);
        delete t;
    }
    
    pthread_key_t key_;

    per_thread(const per_thread& rhs);
    void operator=(const per_thread& rhs);
};

#endif
