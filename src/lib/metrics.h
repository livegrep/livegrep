/********************************************************************
 * livegrep -- metrics.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_METRICS_H
#define CODESEARCH_METRICS_H

#include "atomic.h"
#include "timer.h"

#include <string>

class metric {
public:
    metric(const std::string &name);
    void inc() {++val_;}
    void inc(long i) {val_ += i;}
    void dec() {--val_;}
    void dec(long i) {val_ -= i;}

    static void dump_all();
private:
    atomic_long val_;
};

class record_time {
    record_time() {}

    ~record_time() {
        tm_.pause();
        timeval elapsed = tm_.elapsed();
        m_->inc(elapsed.tv_sec * 1000 + elapsed.tv_usec / 1000);
    }
private:
    metric *m_;
    timer tm_;
};

#endif
