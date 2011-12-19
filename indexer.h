#ifndef CODESEARCH_INDEXER_H
#define CODESEARCH_INDEXER_H

#include <vector>
#include <list>
#include <string>
#include <memory>

#include "re2/re2.h"
#include "re2/walker-inl.h"

#include "common.h"

using std::string;
using std::vector;
using std::list;
using std::shared_ptr;

enum {
    kAnchorNone   = 0x00,
    kAnchorLeft   = 0x01,
    kAnchorRight  = 0x02,
    kAnchorBoth   = 0x03,
    kAnchorRepeat = 0x04
};

class IndexKey {
public:
    typedef map<pair<uchar, uchar>, shared_ptr<IndexKey> >::iterator iterator;
    typedef map<pair<uchar, uchar>, shared_ptr<IndexKey> >::const_iterator const_iterator;
    typedef pair<pair<uchar, uchar>, shared_ptr<IndexKey> > value_type;

    iterator begin() {
        return edges_.begin();
    }

    iterator end() {
        return edges_.end();
    }

    IndexKey(int anchor = kAnchorNone) : anchor(anchor) { }

    IndexKey(pair<uchar, uchar> p,
             shared_ptr<IndexKey> next,
             int anchor = kAnchorNone) : anchor(anchor) {
        edges_.insert(value_type(p, next));
    }

    void insert(const value_type& v) {
        edges_.insert(v);
    }

    bool empty() {
        return edges_.empty();
    }

    size_t size() {
        return edges_.size();
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

    string ToString();

    void check_rep();

    void concat(shared_ptr<IndexKey> rhs);

    int anchor;
protected:
    map<pair<uchar, uchar>, shared_ptr<IndexKey> > edges_;

    void collect_tails(list<IndexKey::iterator>& tails);

private:
    IndexKey(const IndexKey&);
    void operator=(const IndexKey&);
};

shared_ptr<IndexKey> indexRE(const re2::RE2 &pat);

#endif /* CODESEARCH_INDEXER_H */
