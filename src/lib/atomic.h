/********************************************************************
 * livegrep -- atomic.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_ATOMIC_H
#define CODESEARCH_ATOMIC_H

/* A minimal implementation of atomic_int for portability */

template <class I>
class atomic_integral {
public:
    atomic_integral() : val_(0) { }
    atomic_integral(I x) : val_(x) { }

    I load() {
        return val_;
    }

    I operator++() {
        return __sync_add_and_fetch(&val_, 1);
    }

    I operator--() {
        return __sync_sub_and_fetch(&val_, 1);
    }

    I operator+=(I rhs) {
        return __sync_add_and_fetch(&val_, rhs);
    }

    I operator-=(I rhs) {
        return __sync_sub_and_fetch(&val_, rhs);
    }
private:
    I val_;
};

typedef atomic_integral<int> atomic_int;
typedef atomic_integral<long> atomic_long;

#endif
