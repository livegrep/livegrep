/********************************************************************
 * livegrep -- mutex.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_MUTEX_H
#define CODESEARCH_MUTEX_H

#include <pthread.h>

class cond_var;

class cs_mutex {
public:
    cs_mutex () {
        pthread_mutex_init(&mutex_, NULL);
    }

    ~cs_mutex () {
        pthread_mutex_destroy(&mutex_);
    }

    void lock () {
        pthread_mutex_lock(&mutex_);
    }

    void unlock () {
        pthread_mutex_unlock(&mutex_);
    }
protected:
    cs_mutex(const cs_mutex&);
    cs_mutex operator=(const cs_mutex&);
    pthread_mutex_t mutex_;

    friend class cond_var;
};

class cond_var {
public:
    cond_var() {
        pthread_cond_init(&cond_, NULL);
    }

    ~cond_var() {
        pthread_cond_destroy(&cond_);
    }

    void wait(cs_mutex *mutex) {
        pthread_cond_wait(&cond_, &mutex->mutex_);
    }

    void signal() {
        pthread_cond_signal(&cond_);
    }

    void broadcast() {
        pthread_cond_broadcast(&cond_);
    }
protected:
    pthread_cond_t cond_;
};

class mutex_locker {
public:
    mutex_locker(cs_mutex& mutex)
        : mutex_(mutex) {
        mutex_.lock();
    }

    ~mutex_locker() {
        mutex_.unlock();
    }
 protected:

    mutex_locker(const mutex_locker& rhs);
    mutex_locker operator=(const mutex_locker &rhs);

    cs_mutex &mutex_;
};


#endif /* !defined(CODESEARCH_MUTEX_H) */
