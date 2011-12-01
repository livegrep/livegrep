#include "indexer.h"

#include <gflags/gflags.h>

#include <stdarg.h>

DEFINE_bool(debug_index, false, "Debug the index query generator.");

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
const int kMaxFilters     = 32;

double IndexKey::selectivity() {
    if (keys.size() == 0)
        return 1.0;

    double s = 0.0;
    for (vector<string>::const_iterator it = keys.begin();
         it != keys.end(); ++it)
        s += pow(1./64, min(it->size(), size_t(4)));

    return s;
}

unsigned IndexKey::weight() {
    return 1/selectivity();
}

string IndexKey::ToString() {
    string out;
    for (vector<string>::const_iterator it = keys.begin();
         it != keys.end(); ++it) {
        out += *it;
        out += ",";
    }
    out += "|";
    if (anchor & kAnchorLeft)
        out += "<";
    if (anchor & kAnchorRight)
        out += ">";
    return out;
}

class IndexWalker : public Regexp::Walker<IndexKey*> {
public:
    IndexWalker() { }
    virtual IndexKey *
    PostVisit(Regexp* re, IndexKey *parent_arg,
              IndexKey *pre_arg,
              IndexKey **child_args, int nchild_args);

    virtual IndexKey *
    ShortVisit(Regexp* re,
               IndexKey *parent_arg);

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

    IndexKey *Any() {
        return new IndexKey(kAnchorNone);
    }

    IndexKey *Empty() {
        return new IndexKey(kAnchorBoth);
    }


    IndexKey *Literal(Rune r) {
        IndexKey *k = new IndexKey;
        k->keys.push_back(RuneToString(r));
        k->anchor = kAnchorBoth;
        return k;
    }

    IndexKey *Literal(Rune *runes, int nrunes) {
        IndexKey *k = new IndexKey;
        string lit;

        for (int i = 0; i < nrunes; i++) {
            lit.append(RuneToString(runes[i]));
        }

        k->keys.push_back(lit);
        k->anchor = kAnchorBoth;

        return k;
    }

    IndexKey *CClass(CharClass *cc) {
        if (cc->size() > kMaxFilters)
            return Any();

        IndexKey *k = new IndexKey();

        for (CharClass::iterator i = cc->begin(); i != cc->end(); ++i)
            for (Rune r = i->lo; r <= i->hi; r++)
                k->keys.push_back(RuneToString(r));

        k->anchor = kAnchorBoth;

        return k;
    }

    IndexKey *Concat(IndexKey *lhs, IndexKey *rhs) {
        IndexKey *out = 0;

        debug("Concat([%s](%d), [%s](%d)) = ",
               lhs->ToString().c_str(),
               lhs->weight(),
               rhs->ToString().c_str(),
               rhs->weight());

        if ((lhs->anchor & kAnchorRight) &&
            (rhs->anchor & kAnchorLeft) &&
            lhs->keys.size() && rhs->keys.size() &&
            lhs->keys.size() * rhs->keys.size() <= kMaxFilters) {
            out = new IndexKey;
            for (vector<string>::iterator lit = lhs->keys.begin();
                 lit != lhs->keys.end(); ++lit)
                for (vector<string>::iterator rit = rhs->keys.begin();
                     rit != rhs->keys.end(); ++rit)
                    out->keys.push_back(*lit + *rit);
            out->anchor = (lhs->anchor & kAnchorLeft) | (rhs->anchor & kAnchorRight);
        }

        if (!out || lhs->weight() > out->weight()) {
            delete out;
            out = lhs;
            out->anchor &= ~kAnchorRight;
        } else {
            delete lhs;
        }

        if (rhs->weight() > out->weight()) {
            delete out;
            out = rhs;
            out->anchor &= ~kAnchorLeft;
        } else {
            delete rhs;
        }

        debug("[%s]\n", out->ToString().c_str());

        return out;
    }

    IndexKey *Alternate(IndexKey *lhs, IndexKey *rhs) {
        if (lhs->keys.size() + rhs->keys.size() < kMaxFilters) {
            lhs->keys.insert(lhs->keys.end(), rhs->keys.begin(), rhs->keys.end());
            lhs->anchor &= rhs->anchor;

            delete rhs;
            return lhs;
        }
        delete lhs;
        delete rhs;

        return Any();
    }

};

unique_ptr<IndexKey> indexRE(const re2::RE2 &re) {
    IndexWalker walk;

    unique_ptr<IndexKey> key(walk.Walk(re.Regexp(), 0));

    if (key->weight() < kMinWeight)
        key->keys.clear();
    return key;
}

IndexKey *IndexWalker::PostVisit(Regexp* re, IndexKey *parent_arg,
                                 IndexKey *pre_arg,
                                 IndexKey **child_args, int nchild_args) {
    IndexKey *key;

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
        delete child_args[0];
        key = Any();
        break;

    case kRegexpCapture:
        key = child_args[0];
        break;

    case kRegexpRepeat:
    case kRegexpPlus:
        key = child_args[0];
        if (key->anchor == kAnchorBoth)
            key->anchor &= ~kAnchorRight;
        break;

    default:
        assert(false);
    }

    debug(" %s -> [%s](%d)\n",
          re->ToString().c_str(),
          key->ToString().c_str(),
          key->weight());

    return key;
}

IndexKey *IndexWalker::ShortVisit(Regexp* re, IndexKey *parent_arg) {
    return Any();
}
