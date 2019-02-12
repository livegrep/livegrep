/********************************************************************
 * livegrep -- query_planner.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "src/lib/recursion.h"
#include "src/lib/debug.h"

#include "src/query_planner.h"

#include <gflags/gflags.h>

#include <list>
#include <limits>

#include <stdarg.h>

using namespace re2;
using namespace std;

const unsigned kMinWeight = 16;
const int kMaxWidth       = 32;
const int kMaxRecursion   = 10;
const int kMaxNodes       = (1 << 24);

namespace {
    static QueryPlan::Stats null_stats;
};

QueryPlan::Stats::Stats ()
    : selectivity_(1.0), depth_(0), nodes_(1), tail_paths_(1) {
}

QueryPlan::Stats QueryPlan::Stats::insert(const value_type& val) const {
    Stats out(*this);
    if (out.selectivity_ == 1.0) {
        out.selectivity_ = 0.0;
        out.tail_paths_  = 0;
    }

    const Stats& rstats = val.second ? val.second->stats() : null_stats;

    // There are 100 printable ASCII characters. As a zeroth-order
    // approximation, assume our corpus is random strings of printable
    // ASCII characters.  The exact computation of selectivity turn
    // out not to matter all that much in most cases.
    out.selectivity_ += (val.first.second - val.first.first + 1)/100. * rstats.selectivity_;
    out.depth_ = max(depth_, rstats.depth_ + 1);
    out.nodes_ += (val.first.second - val.first.first + 1) * rstats.nodes_;
    if (!val.second)
        out.tail_paths_ += (val.first.second - val.first.first + 1);

    return out;
}

QueryPlan::Stats QueryPlan::Stats::concat(const QueryPlan::Stats& rhs) const {
    Stats out(*this);
    out.selectivity_ *= rhs.selectivity_;
    out.depth_ += rhs.depth_;
    out.nodes_ += tail_paths_ * rhs.nodes_;
    out.tail_paths_ *= rhs.tail_paths_;

    return out;
}

void QueryPlan::insert(const value_type& val) {
    stats_ = stats_.insert(val);

    edges_.insert(val);
    if (val.second && !(val.second->anchor & kAnchorRight)) {
        anchor &= ~kAnchorRight;
    }
}

double QueryPlan::selectivity() {
    if (empty())
        assert(stats_.selectivity_ == 1.0);

    return stats_.selectivity_;
}

unsigned QueryPlan::weight() {
    if (1/selectivity() > double(numeric_limits<unsigned>::max()))
        return numeric_limits<unsigned>::max() / 2;
    return 1/selectivity();
}

long QueryPlan::nodes() {
    return stats_.nodes_;
}

int QueryPlan::depth() {
    return stats_.depth_;
}

void QueryPlan::collect_tails(list<QueryPlan::iterator>& tails) {
    set<QueryPlan*> seen;
    collect_tails(tails, seen);
}

void QueryPlan::collect_tails(list<QueryPlan::const_iterator>& tails) {
    list<iterator> tmp;
    collect_tails(tmp);
    tails.insert(tails.end(), tmp.begin(), tmp.end());
}

void QueryPlan::collect_tails(list<QueryPlan::iterator>& tails,
                             set<QueryPlan*> &seen) {
    if (seen.find(this) != seen.end())
        return;
    seen.insert(this);

    for (auto it = begin(); it != end(); ++it) {
        if (it->second)
            it->second->collect_tails(tails, seen);
        else
            tails.push_back(it);
    }
}

void QueryPlan::concat(intrusive_ptr<QueryPlan> rhs) {
    assert(anchor & kAnchorRight);
    assert(rhs->anchor & kAnchorLeft);
    assert(!empty());

    if (rhs->empty())
        return;

    list<QueryPlan::iterator> tails;
    collect_tails(tails);
    for (auto it = tails.begin(); it != tails.end(); ++it) {
        assert(!(*it)->second);
        (*it)->second = rhs;
    }
    if (anchor & kAnchorRepeat)
        anchor &= ~kAnchorLeft;
    if ((rhs->anchor & (kAnchorRepeat|kAnchorRight)) != kAnchorRight)
        anchor &= ~kAnchorRight;

    stats_ = stats_.concat(rhs->stats());
}

static string StrChar(uchar c) {
    if (c > ' ' && c <= '~')
        return strprintf("%c", c);
    return strprintf("\\x%02x", c);
}

static string ToString(QueryPlan *k, int indent = 0) {
    string out;
    if (k == 0)
        return strprintf("%*.s[null]\n", indent, "");
    if (k->empty())
        return strprintf("%*.s[]\n", indent, "");

    for (QueryPlan::iterator it = k->begin(); it != k->end(); ++it) {
        out += strprintf("%*.s[%s-%s] -> \n",
                         indent, "",
                         StrChar(it->first.first).c_str(),
                         StrChar(it->first.second).c_str());
        out += ToString(it->second.get(), indent + 1);
    }
    return out;
}

string QueryPlan::ToString() {
    string out = ::ToString(this, 0);

    out += "|";
    if (anchor & kAnchorLeft)
        out += "<";
    if (anchor & kAnchorRepeat)
        out += "*";
    if (anchor & kAnchorRight)
        out += ">";
    return out;
}

class IndexWalker : public Regexp::Walker<intrusive_ptr<QueryPlan> > {
public:
    IndexWalker() { }
    virtual intrusive_ptr<QueryPlan>
    PostVisit(Regexp* re, intrusive_ptr<QueryPlan> parent_arg,
              intrusive_ptr<QueryPlan> pre_arg,
              intrusive_ptr<QueryPlan> *child_args, int nchild_args);

    virtual intrusive_ptr<QueryPlan>
    ShortVisit(Regexp* re,
               intrusive_ptr<QueryPlan> parent_arg);

private:
    IndexWalker(const IndexWalker&);
    void operator=(const IndexWalker&);
};

namespace {
    typedef map<pair<intrusive_ptr<QueryPlan>, intrusive_ptr<QueryPlan> >,
                intrusive_ptr<QueryPlan> > alternate_cache;

    intrusive_ptr<QueryPlan> Alternate(alternate_cache&,
                                   intrusive_ptr<QueryPlan>,
                                   intrusive_ptr<QueryPlan>);

    string RuneToString(Rune r) {
        char buf[UTFmax];
        int n = runetochar(buf, &r);
        return string(buf, n);
    }

    intrusive_ptr<QueryPlan> Any() {
        return intrusive_ptr<QueryPlan>(new QueryPlan());
    }

    intrusive_ptr<QueryPlan> Empty() {
        return intrusive_ptr<QueryPlan>(new QueryPlan(kAnchorBoth));
    }

    intrusive_ptr<QueryPlan> Literal(string s) {
        intrusive_ptr<QueryPlan> k = 0;
        for (string::reverse_iterator it = s.rbegin();
             it != s.rend(); ++it) {
            k = intrusive_ptr<QueryPlan>(new QueryPlan(pair<uchar, uchar>(*it, *it), k));
        }
        k->anchor = kAnchorBoth;
        return k;
    }

    intrusive_ptr<QueryPlan> Literal(Rune r) {
        return Literal(RuneToString(r));
    }

    intrusive_ptr<QueryPlan> CaseFoldLiteral(Rune r) {
        if (r > 127)
            return Any();
        if (r < 'a' || r > 'z') {
            return Literal(r);
        }
        intrusive_ptr<QueryPlan> k(new QueryPlan(kAnchorBoth));
        k->insert(make_pair(make_pair((uchar)r, (uchar)r), (QueryPlan*)0));
        k->insert(make_pair(make_pair((uchar)r - 'a' + 'A',
                                      (uchar)r - 'a' + 'A'), (QueryPlan*)0));
        return k;
    }

    intrusive_ptr<QueryPlan> Literal(Rune *runes, int nrunes) {
        string lit;

        for (int i = 0; i < nrunes; i++) {
            lit.append(RuneToString(runes[i]));
        }

        return Literal(lit);
    }

    intrusive_ptr<QueryPlan> Concat(intrusive_ptr<QueryPlan> *children, int nchildren);
    intrusive_ptr<QueryPlan> CaseFoldLiteral(Rune *runes, int nrunes) {
        if (nrunes == 0)
            return Empty();
        std::vector<intrusive_ptr<QueryPlan> > keys;
        for (int i = 0; i < nrunes; ++i) {
            keys.push_back(CaseFoldLiteral(runes[i]));
        }
        return Concat(&keys[0], nrunes);
    }

    intrusive_ptr<QueryPlan> LexRange(const string &lo, const string& hi) {
        if (lo.size() == 0 && hi.size() == 0)
            return intrusive_ptr<QueryPlan>();
        if (lo.size() == 0)
            return Literal(hi);

        intrusive_ptr<QueryPlan> out(new QueryPlan(kAnchorBoth));
        assert(hi.size() != 0);
        if (lo[0] < hi[0])
            out->insert(QueryPlan::value_type
                        (pair<uchar, uchar>(lo[0], hi[0] - 1), (QueryPlan*)0));
        out->insert(QueryPlan::value_type
                    (pair<uchar, uchar>(hi[0], hi[0]),
                     LexRange(lo.substr(1), hi.substr(1))));
        return out;
    }

    intrusive_ptr<QueryPlan> CClass(CharClass *cc) {
        if (cc->size() > kMaxWidth)
            return Any();

        intrusive_ptr<QueryPlan> k(new QueryPlan(kAnchorBoth));

        for (CharClass::iterator i = cc->begin(); i != cc->end(); ++i) {
            if (i->hi < Runeself)
                k->insert(QueryPlan::value_type
                          (pair<uchar, uchar>(i->lo, i->hi),
                           (QueryPlan*)0));
            else {
                alternate_cache cache;
                k = Alternate(cache, k, LexRange(RuneToString(i->lo),
                                                 RuneToString(i->hi)));
            }
        }

        return k;
    }

    bool ShouldConcat(intrusive_ptr<QueryPlan> lhs, intrusive_ptr<QueryPlan> rhs) {
        assert(lhs && rhs);
        if (!(lhs->anchor & kAnchorRight) ||
            !(rhs->anchor & kAnchorLeft))
            return false;
        if (lhs->empty())
            return false;
        QueryPlan::Stats concat = lhs->stats().concat(rhs->stats());
        if (concat.nodes_ >= kMaxNodes)
            return false;
        return true;
    }

    bool Prefer(const QueryPlan::Stats& lhs,
                const QueryPlan::Stats& rhs) {
        return (lhs.selectivity_ < rhs.selectivity_);
        /*
        return (kRECost * lhs.selectivity_ + kNodeCost * lhs.nodes_ <
                kRECost * rhs.selectivity_ + kNodeCost * rhs.nodes_);
        */
    }

    intrusive_ptr<QueryPlan> Concat(intrusive_ptr<QueryPlan> lhs, intrusive_ptr<QueryPlan> rhs) {
        assert(lhs);
        intrusive_ptr<QueryPlan> out = lhs;

        debug(kDebugIndexAll,
              "Concat([%s](%ld), [%s](%ld)) = ",
              lhs ? lhs->ToString().c_str() : "",
              lhs->nodes(),
              rhs ? rhs->ToString().c_str() : "",
              rhs->nodes());

        if (ShouldConcat(lhs, rhs)) {
            out->concat(rhs);
        } else if(Prefer(lhs->stats(), rhs->stats()))  {
            out->anchor &= ~kAnchorRight;
        } else {
            out = rhs;
            out->anchor &= ~kAnchorLeft;
        }

        debug(kDebugIndexAll, "[%s]", out->ToString().c_str());

        return out;
    }

    QueryPlan::Stats TryConcat(intrusive_ptr<QueryPlan> *start,
                              intrusive_ptr<QueryPlan> *end) {
        QueryPlan::Stats st = (*start)->stats();
        debug(kDebugIndexAll, "TryConcat: Searching suffix of length %d",
              int(end - start));
        if (!*start || !((*start)->anchor & kAnchorRight) || (*start)->empty()) {
            debug(kDebugIndexAll, "!ConcatRight, returning early.");
            return st;
        }
        for (intrusive_ptr<QueryPlan> *ptr = start + 1; ptr != end; ptr++) {
            if (!*(ptr) || !((*ptr)->anchor & kAnchorLeft))
                break;
            QueryPlan::Stats concat = st.concat((*ptr)->stats());

            if (concat.nodes_ >= kMaxNodes)
                break;

            st = concat;

            if (((*ptr)->anchor & (kAnchorRepeat|kAnchorRight)) != kAnchorRight)
                break;
        }
        debug(kDebugIndexAll, "TryConcat: nodes=%ld, selectivity=%f",
              st.nodes_, st.selectivity_);
        return st;
    }

    intrusive_ptr<QueryPlan> Concat(intrusive_ptr<QueryPlan> *children,
                                int nchildren) {
        intrusive_ptr<QueryPlan> *end = children + nchildren, *best_start = 0, *ptr;
        QueryPlan::Stats best_stats;

        debug(kDebugIndexAll, "Concat: Searching %d positions", nchildren);
        for (ptr = children; ptr != end; ptr++) {
            QueryPlan::Stats st = TryConcat(ptr, end);
            if (st.nodes_ > 1 && Prefer(st, best_stats)) {
                debug(kDebugIndexAll, "Concat: Found new best: %d: %f",
                      int(ptr - children), st.selectivity_);
                best_start = ptr;
                best_stats = st;
            }
        }

        if (best_start == 0) {
            debug(kDebugIndexAll, "Concat: No good results found.");
            return Any();
        }

        intrusive_ptr<QueryPlan> out = *best_start;
        for (ptr = best_start + 1; ptr != end; ptr++) {
            out = Concat(out, *ptr);
        }
        if (best_start != children)
            out->anchor &= ~kAnchorLeft;
        return out;
    }

    bool intersects(const pair<uchar, uchar>& left,
                    const pair<uchar, uchar>& right) {
        if (left.first <= right.first)
            return left.second >= right.first;
        else
            return right.second >= left.first;
    }

    enum {
        kTakeLeft  = 0x01,
        kTakeRight = 0x02,
        kTakeBoth  = 0x03
    };

    int Merge(alternate_cache& cache,
              intrusive_ptr<QueryPlan> out,
              pair<uchar, uchar>& left,
              intrusive_ptr<QueryPlan> lnext,
              pair<uchar, uchar>& right,
              intrusive_ptr<QueryPlan> rnext) {
        if (intersects(left, right)) {
            debug(kDebugIndexAll,
                  "Processing intersection: <%hhx,%hhx> vs. <%hhx,%hhx>",
                  left.first, left.second, right.first, right.second);
            if (left.first < right.first) {
                out->insert
                    (make_pair(make_pair(left.first,
                                         right.first - 1),
                               lnext));
                left.first = right.first;
            } else if (right.first < left.first) {
                out->insert
                    (make_pair(make_pair(right.first,
                                         left.first - 1),
                               rnext));
                right.first = left.first;
            }
            /* left and right now start at the same location */
            assert(left.first == right.first);

            uchar end = min(left.second, right.second);
            out->insert
                (make_pair(make_pair(left.first, end),
                           Alternate(cache, lnext, rnext)));
            if (left.second > end) {
                left.first = end+1;
                return kTakeRight;
            } else if (right.second > end) {
                right.first = end+1;
                return kTakeLeft;
            }
            return kTakeBoth;
        }
        /* Non-intersecting */
        if (left.first < right.first) {
            out->insert(make_pair(left, lnext));
            return kTakeLeft;
        }
        assert(right.first < left.first);
        out->insert(make_pair(right, rnext));
        return kTakeRight;
    }

    intrusive_ptr<QueryPlan> AlternateInternal(alternate_cache& cache,
                                           intrusive_ptr<QueryPlan> lhs,
                                           intrusive_ptr<QueryPlan> rhs) {
        if (lhs == rhs)
            return lhs;
        if (!lhs || !rhs ||
            lhs->empty() || rhs->empty() ||
            lhs->size() + rhs->size() >= kMaxWidth)
            return Any();

        static int recursion_depth = 0;
        RecursionCounter guard(recursion_depth);
        if (recursion_depth > kMaxRecursion)
            return Any();

        intrusive_ptr<QueryPlan> out(new QueryPlan(lhs->anchor & rhs->anchor & (kAnchorLeft|kAnchorRight)));
        QueryPlan::const_iterator lit, rit;
        lit = lhs->begin();
        rit = rhs->begin();
        pair<uchar, uchar> left;
        if (lit != lhs->end())
            left = lit->first;
        pair<uchar, uchar> right = rit->first;
        if (rit != rhs->end())
            right = rit->first;
        while (lit != lhs->end() && rit != rhs->end()) {
            int action = Merge(cache, out, left, lit->second, right, rit->second);
            if (action & kTakeLeft)
                if (++lit != lhs->end())
                    left = lit->first;
            if (action & kTakeRight)
                if (++rit != rhs->end())
                    right = rit->first;
        }

        if (lit != lhs->end()) {
            out->insert(make_pair(left, lit->second));
            ++lit;
        }
        if (rit != rhs->end()) {
            out->insert(make_pair(right, rit->second));
            ++rit;
        }

        for (; lit != lhs->end(); ++lit)
            out->insert(*lit);
        for (; rit != rhs->end(); ++rit)
            out->insert(*rit);

        return out;
    }

    intrusive_ptr<QueryPlan> Alternate(alternate_cache& cache,
                                   intrusive_ptr<QueryPlan> lhs,
                                   intrusive_ptr<QueryPlan> rhs) {
        auto it = cache.find(make_pair(lhs, rhs));
        if (it != cache.end())
            return it->second;
        intrusive_ptr<QueryPlan> out = AlternateInternal(cache, lhs, rhs);
        cache[make_pair(lhs, rhs)] = out;
        return out;
    }

};

intrusive_ptr<QueryPlan> constructQueryPlan(const re2::RE2 &re) {
    IndexWalker walk;

    Regexp *sre = re.Regexp()->Simplify();
    intrusive_ptr<QueryPlan> key = walk.WalkExponential(sre, 0, 10000);
    sre->Decref();

    if (key && key->weight() < kMinWeight)
        key = 0;
    return key;
}

intrusive_ptr<QueryPlan>
IndexWalker::PostVisit(Regexp* re, intrusive_ptr<QueryPlan> parent_arg,
                       intrusive_ptr<QueryPlan> pre_arg,
                       intrusive_ptr<QueryPlan> *child_args,
                       int nchild_args) {
    intrusive_ptr<QueryPlan> key;

    switch (re->op()) {
    case kRegexpNoMatch:
    case kRegexpEmptyMatch:      // anywhere
    case kRegexpBeginLine:       // at beginning of line
    case kRegexpEndLine:         // at end of line
    case kRegexpBeginText:       // at beginning of text
    case kRegexpEndText:         // at end of text
    case kRegexpWordBoundary:    // at word boundary
    case kRegexpNoWordBoundary:  // not at word boundary
        key = Empty();
        break;

    case kRegexpAnyChar:
    case kRegexpAnyByte:
        key = Any();
        break;

    case kRegexpLiteral:
        if (re->parse_flags() & Regexp::FoldCase) {
            key = CaseFoldLiteral(re->rune());
        } else {
            key = Literal(re->rune());
        }
        break;

    case kRegexpCharClass:
        key = CClass(re->cc());
        break;

    case kRegexpLiteralString:
        if (re->parse_flags() & Regexp::FoldCase) {
            key = CaseFoldLiteral(re->runes(), re->nrunes());
        } else {
            key = Literal(re->runes(), re->nrunes());
        }
        break;

    case kRegexpConcat:
        key = Concat(child_args, nchild_args);
        break;

    case kRegexpAlternate:
        {
            alternate_cache cache;
            key = child_args[0];
            for (int i = 1; i < nchild_args; i++)
                key = Alternate(cache, key, child_args[i]);
        }
        break;

    case kRegexpStar:
    case kRegexpQuest:
        key = Any();
        break;

    case kRegexpCapture:
        key = child_args[0];
        break;

    case kRegexpRepeat:
        if (re->min() == 0)
            return Any();
    case kRegexpPlus:
        key = child_args[0];
        if ((key->anchor & kAnchorBoth) == kAnchorBoth)
            key->anchor |= kAnchorRepeat;
        break;

    default:
        assert(false);
    }

    assert(key);

    debug(kDebugIndex, "* INDEX %s ==> ", re->ToString().c_str());
    if (key)
        debug(kDebugIndex, "[weight %d, nodes %ld, depth %d]",
              key->weight(), key->nodes(), key->depth());
    else
        debug(kDebugIndex, "nul");

    debug(kDebugIndexAll, "           ==> [%s]",
          key ? key->ToString().c_str() : "nul");

    if ((debug_enabled & kDebugIndex) && key)
        key->check_rep();

    return key;
}

intrusive_ptr<QueryPlan>
IndexWalker::ShortVisit(Regexp* re, intrusive_ptr<QueryPlan> parent_arg) {
    return Any();
}

void QueryPlan::check_rep() {
    pair<uchar, uchar> last = make_pair('\0', '\0');
    for (iterator it = begin(); it != end(); ++it) {
        assert(!intersects(last, it->first));
        last = it->first;
    }
}

void intrusive_ptr_add_ref(QueryPlan *key) {
    ++key->refs_;
}

void intrusive_ptr_release(QueryPlan *key) {
    if (--key->refs_ == 0)
        delete key;
}
