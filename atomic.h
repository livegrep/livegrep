#ifndef CODESEARCH_ATOMIC_H
#define CODESEARCH_ATOMIC_H

/* A minimal implementation of atomic_int for portability */

class atomic_int {
public:
    atomic_int(int x) : val_(x) { }

    int load() {
        return val_;
    }

    int operator++() {
        return __sync_fetch_and_add(&val_, 1);
    }
private:
    int val_;
};

#endif
