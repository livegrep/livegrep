/********************************************************************
 * livegrep -- recursion.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_RECURSION_H
#define CODESEARCH_RECURSION_H

class RecursionCounter {
public:
    RecursionCounter(int &depth) : depth_(depth) {
        depth_++;
    }
    ~RecursionCounter() {
        depth_--;
    }
protected:
    int &depth_;
};

#endif
