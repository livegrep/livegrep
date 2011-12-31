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
