#include "content.h"
#include "chunk.h"


void file_contents::extend(chunk *c, const StringPiece &piece) {
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
