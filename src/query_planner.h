/********************************************************************
 * livegrep -- query_planner.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_INDEXER_H
#define CODESEARCH_INDEXER_H

#include <vector>
#include <list>
#include <set>
#include <string>
#include <atomic>
#include <map>
#include <boost/intrusive_ptr.hpp>

#include "re2/re2.h"
#include "re2/walker-inl.h"

#include "src/common.h"

using std::string;
using std::vector;
using std::list;
using std::set;
using boost::intrusive_ptr;

enum {
    kAnchorNone   = 0x00,
    kAnchorLeft   = 0x01,
    kAnchorRight  = 0x02,
    kAnchorBoth   = 0x03,
    kAnchorRepeat = 0x04
};

class QueryPlan {
public:
    typedef std::map<std::pair<uchar, uchar>, intrusive_ptr<QueryPlan> >::iterator iterator;
    typedef std::map<std::pair<uchar, uchar>, intrusive_ptr<QueryPlan> >::const_iterator const_iterator;
    typedef std::pair<std::pair<uchar, uchar>, intrusive_ptr<QueryPlan> > value_type;

    iterator begin() {
        return edges_.begin();
    }

    iterator end() {
        return edges_.end();
    }

    QueryPlan(int anchor = kAnchorNone)
        : anchor(anchor), refs_(0) { }

    QueryPlan(std::pair<uchar, uchar> p,
             intrusive_ptr<QueryPlan> next,
             int anchor = kAnchorNone)
        : anchor(anchor), refs_(0) {
        insert(value_type(p, next));
    }

    void insert(const value_type& v);
    void concat(intrusive_ptr<QueryPlan> rhs);

    bool empty() {
        return edges_.empty();
    }

    size_t size() {
        return edges_.size();
    }

    class Stats {
    public:
        double selectivity_;
        int depth_;
        long nodes_;
        long tail_paths_;

        Stats();
        Stats insert(const value_type& v) const;
        Stats concat(const Stats& rhs) const;
    };

    const Stats& stats() {
        return stats_;
    }

    /*
     * Returns an approximation of the fraction of the input corpus
     * that this index key will reduce the search space to.
     *
     * e.g. selectivity() == 1.0 implies that this index key includes
     *      the entire input.
     *
     *      selectivity() == 0.1 means that using this index key will
     *      only require searching 1/10th of the corpus.
     *
     * This value is computed without any reference to the actual
     * characteristics of any particular corpus, and so is a rough
     * approximation at best.
     */
    double selectivity();

    /*
     * Returns a value approximating the "goodness" of this index key,
     * in arbitrary units. Higher is better. The weight incorporates
     * both the selectivity, above, and the cost of using this index
     * key.
     */
    unsigned weight();
    int depth();
    long nodes();

    string ToString();

    void check_rep();

    int anchor;

    void collect_tails(list<QueryPlan::const_iterator>& tails);
protected:
    std::map<std::pair<uchar, uchar>, intrusive_ptr<QueryPlan> > edges_;
    Stats stats_;
    std::atomic_int refs_;

    void collect_tails(list<QueryPlan::iterator>& tails);
    void collect_tails(list<QueryPlan::iterator>& tails,
                       set<QueryPlan*> &seen);

private:
    QueryPlan(const QueryPlan&);
    void operator=(const QueryPlan&);

    friend void intrusive_ptr_add_ref(QueryPlan *key);
    friend void intrusive_ptr_release(QueryPlan *key);
};

intrusive_ptr<QueryPlan> constructQueryPlan(const re2::RE2 &pat);

#endif /* CODESEARCH_INDEXER_H */
