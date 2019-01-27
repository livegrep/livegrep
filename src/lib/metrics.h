/********************************************************************
 * livegrep -- metrics.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_METRICS_H
#define CODESEARCH_METRICS_H

#include "timer.h"

#include <atomic>
#include <string>

class metric {
public:
    metric(const std::string &name);
    void inc() {++val_;}
    void inc(long i) {val_ += i;}
    void dec() {--val_;}
    void dec(long i) {val_ -= i;}

    static void dump_all();

#ifdef CODESEARCH_SLOWGTOD
    class timer {
    public:
        timer(metric &m)  {}
        void pause() {};
        void start() {};
    };
#else
    class timer {
    public:
        timer(metric &m) : m_(&m) {}

        void pause() {
            tm_.pause();
            timeval elapsed = tm_.elapsed();
            m_->inc(elapsed.tv_sec * 1000000 + elapsed.tv_usec);
            tm_.reset();
        }

        void start() {
            tm_.start();
        }

        ~timer() {
            if (tm_.running())
                pause();
        }
    private:
        metric *m_;
        ::timer tm_;
    };
#endif

private:
    std::atomic_long val_;
};

#endif
