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
        s += pow(1./(it->size() + 1), 8);

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
        return shared_ptr<IndexKey>(new IndexKey(kAnchorNone));
    }

    shared_ptr<IndexKey> Empty() {
        return shared_ptr<IndexKey>(new IndexKey(kAnchorBoth));
    }


    shared_ptr<IndexKey> Literal(Rune r) {
        shared_ptr<IndexKey> k(new IndexKey);
        k->keys.push_back(RuneToString(r));
        k->anchor = kAnchorBoth;
        return k;
    }

    shared_ptr<IndexKey> Literal(Rune *runes, int nrunes) {
        shared_ptr<IndexKey> k(new IndexKey);
        string lit;

        for (int i = 0; i < nrunes; i++) {
            lit.append(RuneToString(runes[i]));
        }

        k->keys.push_back(lit);
        k->anchor = kAnchorBoth;

        return k;
    }

    shared_ptr<IndexKey> CClass(CharClass *cc) {
        if (cc->size() > kMaxFilters)
            return Any();

        shared_ptr<IndexKey> k(new IndexKey());

        for (CharClass::iterator i = cc->begin(); i != cc->end(); ++i)
            for (Rune r = i->lo; r <= i->hi; r++)
                k->keys.push_back(RuneToString(r));

        k->anchor = kAnchorBoth;

        return k;
    }

    shared_ptr<IndexKey> Concat(shared_ptr<IndexKey> lhs, shared_ptr<IndexKey> rhs) {
        shared_ptr<IndexKey> out = 0;

        debug("Concat([%s](%d), [%s](%d)) = ",
               lhs->ToString().c_str(),
               lhs->weight(),
               rhs->ToString().c_str(),
               rhs->weight());

        if ((lhs->anchor & kAnchorRight) &&
            (rhs->anchor & kAnchorLeft) &&
            lhs->keys.size() && rhs->keys.size() &&
            lhs->keys.size() * rhs->keys.size() <= kMaxFilters) {
            out = shared_ptr<IndexKey>(new IndexKey);
            for (vector<string>::iterator lit = lhs->keys.begin();
                 lit != lhs->keys.end(); ++lit)
                for (vector<string>::iterator rit = rhs->keys.begin();
                     rit != rhs->keys.end(); ++rit)
                    out->keys.push_back(*lit + *rit);
            if ((lhs->anchor & (kAnchorRepeat|kAnchorLeft)) == kAnchorLeft)
                out->anchor |= kAnchorLeft;
            if ((rhs->anchor & (kAnchorRepeat|kAnchorRight)) == kAnchorRight)
                out->anchor |= kAnchorRight;
        }

        if (!out || lhs->weight() > out->weight()) {
            out = lhs;
            out->anchor &= ~kAnchorRight;
        }

        if (rhs->weight() > out->weight()) {
            out = rhs;
            out->anchor &= ~kAnchorLeft;
        }

        debug("[%s]\n", out->ToString().c_str());

        return out;
    }

    shared_ptr<IndexKey> Alternate(shared_ptr<IndexKey> lhs, shared_ptr<IndexKey> rhs) {
        if (lhs->keys.size() && rhs->keys.size() &&
            lhs->keys.size() + rhs->keys.size() < kMaxFilters) {
            lhs->keys.insert(lhs->keys.end(), rhs->keys.begin(), rhs->keys.end());
            lhs->anchor = (lhs->anchor & rhs->anchor) |
                ((lhs->anchor | lhs->anchor) & kAnchorRepeat);

            return lhs;
        }

        return Any();
    }

};

shared_ptr<IndexKey> indexRE(const re2::RE2 &re) {
    IndexWalker walk;

    shared_ptr<IndexKey> key = walk.Walk(re.Regexp(), 0);

    if (key->weight() < kMinWeight)
        key->keys.clear();
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
    case kRegexpPlus:
        key = child_args[0];
        if ((key->anchor & kAnchorBoth) == kAnchorBoth)
            key->anchor |= kAnchorRepeat;
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

shared_ptr<IndexKey>
IndexWalker::ShortVisit(Regexp* re, shared_ptr<IndexKey> parent_arg) {
    return Any();
}
