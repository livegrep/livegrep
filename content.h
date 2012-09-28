#ifndef CODESEARCH_CONTENT_H
#define CODESEARCH_CONTENT_H

#include <vector>
#include <re2/re2.h>

using re2::StringPiece;
using std::vector;

class file_contents {
public:

    typedef vector<StringPiece>::iterator iterator;

    iterator begin() {
        return pieces.begin();
    }

    iterator end() {
        return pieces.end();
    }

    void extend(const StringPiece &piece) {
        if (pieces.size() &&
            pieces.back().data() + pieces.back().size() == piece.data()) {
            StringPiece &back = pieces.back();
            assert(back.data()[back.size()] == '\n');
            back = StringPiece(back.data(),
                               (piece.data() - back.data() + piece.size()));
        } else {
            pieces.push_back(piece);
        }
    }

    friend class codesearch_index;

protected:
    vector<StringPiece> pieces;
};

#endif
