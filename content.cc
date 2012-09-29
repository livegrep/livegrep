#include "content.h"
#include "chunk.h"

void file_contents::extend(chunk *c, const StringPiece &piece) {
    uint32_t off = reinterpret_cast<const unsigned char*>(piece.data()) - c->data;
    if (pieces.size()) {
        uint32_t id = *(pieces.end() - 3);
        uint32_t end = *(pieces.end() - 2) + *(pieces.end() - 2);
        if (id == c->id &&
            end == off) {
            *(pieces.end() - 1) += piece.size();
            return;
        }
    }

    pieces.push_back(c->id);
    pieces.push_back(off);
    pieces.push_back(piece.size());
}
