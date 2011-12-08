#include "indexer.h"

#include <gflags/gflags.h>

#include <list>

#include <stdarg.h>

DEFINE_bool(debug_index, false, "Debug the index query generator.");
static void debug(const char *format, ...)
    __attribute__((format (printf, 1, 2)));

static void debug(const char *format, ...) {
    if (!FLAGS_debug_index)
        return;
    va_list ap;

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
}

using namespace re2;
using namespace std;

const unsigned kMinWeight = 16;
const int kMaxWidth       = 32;

double IndexKey::selectivity() {
    if (this == 0)
        return 1.0;

    if (edges.size() == 0)
        return 1.0;

    double s = 0.0;
    for (IndexKey::iterator it = begin();
         it != end(); ++it)
        s += double(it->first.second - it->first.first + 1)/128 *
            it->second->selectivity();

    return s;
}

unsigned IndexKey::weight() {
    if (1/selectivity() > double(numeric_limits<unsigned>::max()))
        return numeric_limits<unsigned>::max() / 2;
    return 1/selectivity();
}
static string strprintf(const char *fmt, ...)
    __attribute__((format (printf, 1, 2)));

static string strprintf(const char *fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    return string(buf);
}

static string ToString(IndexKey *k, int indent = 0) {
    string out;
    if (k == 0)
        return strprintf("%*.s[]\n", indent, "");

    for (IndexKey::iterator it = k->begin(); it != k->end(); ++it) {
        out += strprintf("%*.s[%c-%c] -> \n",
                         indent, "",
                         it->first.first,
                         it->first.second);
        out += ToString(it->second.get(), indent + 1);
    }
    return out;
}

string IndexKey::ToString() {
    string out = ::ToString(this, 0);
    if (this == 0)
        return out;

    out += "|";
    if (anchor & kAnchorLeft)
        out += "<";
    if (anchor & kAnchorRepeat)
        out += "*";
    if (anchor & kAnchorRight)
        out += ">";
    return out;
}

class IndexWalker : public Regexp::Walker<shared_ptr<IndexKey> > {
public:
    IndexWalker() { }
    virtual shared_ptr<IndexKey>
    PostVisit(Regexp* re, shared_ptr<IndexKey> parent_arg,
              shared_ptr<IndexKey> pre_arg,
              shared_ptr<IndexKey> *child_args, int nchild_args);

    virtual shared_ptr<IndexKey>
    ShortVisit(Regexp* re,
               shared_ptr<IndexKey> parent_arg);

private:
    IndexWalker(const IndexWalker&);
    void operator=(const IndexWalker&);
};

namespace {
    string RuneToString(Rune r) {
        char buf[UTFmax];
        int n = runetochar(buf, &r);
        return string(buf, n);
    }

    shared_ptr<IndexKey> Any() {
        return shared_ptr<IndexKey>(0);
    }

    shared_ptr<IndexKey> Empty() {
        return shared_ptr<IndexKey>(new IndexKey(kAnchorBoth));
    }

    shared_ptr<IndexKey> Literal(string s) {
        shared_ptr<IndexKey> k = 0;
        for (string::reverse_iterator it = s.rbegin();
             it != s.rend(); ++it) {
            k = shared_ptr<IndexKey>(new IndexKey(pair<uchar, uchar>(*it, *it), k));
        }
        k->anchor = kAnchorBoth;
        return k;
    }

    shared_ptr<IndexKey> Literal(Rune r) {
        return Literal(RuneToString(r));
    }

    shared_ptr<IndexKey> Literal(Rune *runes, int nrunes) {
        string lit;

        for (int i = 0; i < nrunes; i++) {
            lit.append(RuneToString(runes[i]));
        }

        return Literal(lit);
    }

    shared_ptr<IndexKey> CClass(CharClass *cc) {
        if (cc->size() > kMaxWidth)
            return Any();

        shared_ptr<IndexKey> k(new IndexKey(kAnchorBoth));

        for (CharClass::iterator i = cc->begin(); i != cc->end(); ++i) {
            /* TODO: Handle arbitrary unicode ranges. Probably have to
               convert to UTF-8 ranges ourselves.*/
            assert (i->lo < Runeself);
            assert (i->hi < Runeself);
            k->edges.insert(IndexKey::value_type
                            (pair<uchar, uchar>(i->lo, i->hi),
                             0));
        }

        return k;
    }

    void CollectTails(list<IndexKey::iterator>& tails, shared_ptr<IndexKey> key) {
        if (key == 0)
            return;

        for (IndexKey::iterator it = key->begin();
             it != key->end(); ++it) {
            if (!it->second)
                tails.push_back(it);
            else
                CollectTails(tails, it->second);
        }
    }

    shared_ptr<IndexKey> Concat(shared_ptr<IndexKey> lhs, shared_ptr<IndexKey> rhs) {
        shared_ptr<IndexKey> out = lhs;

        debug("Concat([%s](%d), [%s](%d)) = ",
              lhs->ToString().c_str(),
              lhs->weight(),
              rhs->ToString().c_str(),
              rhs->weight());

        if (lhs && rhs &&
            (lhs->anchor & kAnchorRight) &&
            (rhs->anchor & kAnchorLeft) &&
            lhs->edges.size() && rhs->edges.size()) {
            list<IndexKey::iterator> tails;
            CollectTails(tails, lhs);
            for (auto it = tails.begin(); it != tails.end(); ++it) {
                if (!(*it)->second)
                    (*it)->second = rhs;
            }
            if (lhs->anchor & kAnchorRepeat)
                lhs->anchor &= ~kAnchorLeft;
            if ((rhs->anchor & (kAnchorRepeat|kAnchorRight)) != kAnchorRight)
                lhs->anchor &= ~kAnchorRight;

            out = lhs;
        } else if (lhs) {
            lhs->anchor &= ~kAnchorRight;
        }

        if (rhs && rhs->weight() > out->weight()) {
            out = rhs;
            out->anchor &= ~kAnchorLeft;
        }

        debug("[%s]\n", out->ToString().c_str());

        return out;
    }

    bool intersects(const pair<uchar, uchar>& left,
                    const pair<uchar, uchar>& right) {
        if (left.first <= right.first)
            return left.second >= right.first;
        else
            return right.second >= left.first;
    }

    shared_ptr<IndexKey> Alternate(shared_ptr<IndexKey> lhs, shared_ptr<IndexKey> rhs) {
        if (lhs == rhs)
            return lhs;
        if (lhs == 0 || rhs == 0 ||
            lhs->edges.size() + rhs->edges.size() >= kMaxWidth)
            return Any();

        shared_ptr<IndexKey> out(new IndexKey
                                 ((lhs->anchor & rhs->anchor) |
                                  ((lhs->anchor | lhs->anchor) & kAnchorRepeat)));
        IndexKey::const_iterator lit, rit;
        for (lit = lhs->begin(), rit = rhs->begin();
             lit != lhs->end() && rit != rhs->end();) {
            pair<uchar, uchar> left = lit->first;
            pair<uchar, uchar> right = rit->first;

            while (intersects(left, right)) {
                debug("Processing intersection: <%hhx,%hhx> vs. <%hhx,%hhx>\n",
                      left.first, left.second, right.first, right.second);
                if (left.first < right.first) {
                    out->edges.insert
                        (make_pair(make_pair(left.first,
                                             right.first - 1),
                                   lit->second));
                    left.first = right.first;
                } else if (rit->first.first < lit->first.first) {
                    out->edges.insert
                        (make_pair(make_pair(right.first,
                                             left.first - 1),
                                   rit->second));
                    right.first = left.first;
                }
                /* left and right now start at the same location */
                assert(left.first == right.first);

                uchar end = min(left.second, right.second);
                out->edges.insert
                    (make_pair(make_pair(left.first, end),
                               Alternate(lit->second, rit->second)));
                if (left.second > end) {
                    left.first = end+1;
                    if (++rit == rhs->edges.end()) {
                        out->edges.insert(make_pair(left, (lit++)->second));
                        break;
                    }
                    right = rit->first;
                } else if (right.second > end) {
                    right.first = end+1;
                    if (++lit == lhs->edges.end()) {
                        out->edges.insert(make_pair(right, (rit++)->second));
                        break;
                    }
                    left = lit->first;
                } else {
                    left  = (++lit)->first;
                    right = (++rit)->first;
                    break;
                }
            }

            if (lit == lhs->edges.end() || rit == rhs->edges.end())
                break;

            if (left.first < right.first)
                out->edges.insert(make_pair(left, (lit++)->second));
            else if (right.first < left.first)
                out->edges.insert(make_pair(right, (rit++)->second));
            continue;
        }
        for (; lit != lhs->edges.end(); ++lit)
            out->edges.insert(*lit);
        for (; rit != rhs->edges.end(); ++rit)
            out->edges.insert(*rit);

        return out;
    }

};

shared_ptr<IndexKey> indexRE(const re2::RE2 &re) {
    IndexWalker walk;

    shared_ptr<IndexKey> key = walk.Walk(re.Regexp(), 0);

    if (key && key->weight() < kMinWeight)
        key->edges.clear();
    return key;
}

shared_ptr<IndexKey>
IndexWalker::PostVisit(Regexp* re, shared_ptr<IndexKey> parent_arg,
                       shared_ptr<IndexKey> pre_arg,
                       shared_ptr<IndexKey> *child_args,
                       int nchild_args) {
    shared_ptr<IndexKey> key;

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
        key = Literal(re->rune());
        break;

    case kRegexpCharClass:
        key = CClass(re->cc());
        break;

    case kRegexpLiteralString:
        key = Literal(re->runes(), re->nrunes());
        break;

    case kRegexpConcat:
        key = Empty();
        for (int i = 0; i < nchild_args; i++)
            key = Concat(key, child_args[i]);
        break;

    case kRegexpAlternate:
        key = child_args[0];
        for (int i = 1; i < nchild_args; i++)
            key = Alternate(key, child_args[i]);
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

    debug("* INDEX %s ==> [%s](%d)\n",
          re->ToString().c_str(),
          key->ToString().c_str(),
          key->weight());

    return key;
}

shared_ptr<IndexKey>
IndexWalker::ShortVisit(Regexp* re, shared_ptr<IndexKey> parent_arg) {
    return Any();
}
