#ifndef CODESEARCH_CONTENT_H
#define CODESEARCH_CONTENT_H

#include <vector>
#include <re2/re2.h>

using re2::StringPiece;
using std::vector;

struct chunk;

class file_contents {
public:
    class iterator {
    public:
        const StringPiece &operator*() {
            return *it;
        }
        const vector<StringPiece>::iterator &operator->() {
            return it;
        }

        iterator &operator++() {
            ++it;
            return *this;
        }

        iterator &operator--() {
            --it;
            return *this;
        }

        bool operator==(const iterator &rhs) {
            return it == rhs.it;
        }
        bool operator!=(const iterator &rhs) {
            return !(*this == rhs);
        }
    protected:
        iterator(vector<StringPiece>::iterator it) : it(it) {}
        vector<StringPiece>::iterator it;

        friend class file_contents;
    };

    iterator begin() {
        return iterator(pieces.begin());
    }

    iterator end() {
        return iterator(pieces.end());
    }

    void extend(chunk *chunk, const StringPiece &piece);

    friend class codesearch_index;

protected:
    vector<StringPiece> pieces;
};

#endif
