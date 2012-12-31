/********************************************************************
 * livegrep -- content.cc
 * Copyright (c) 2011-2012 Nelson Elhage
 * All Rights Reserved
 ********************************************************************/
#include "content.h"
#include "chunk.h"

void file_contents_builder::extend(chunk *c, const StringPiece &piece) {
    if (pieces_.size()) {
        if (pieces_.back().data() + pieces_.back().size() == piece.data()) {
            pieces_.back().set(pieces_.back().data(),
                               piece.size() + pieces_.back().size());
            return;
        }
    }

    pieces_.push_back(piece);
}

file_contents *file_contents_builder::build(chunk_allocator *alloc) {
    size_t len = sizeof(uint32_t) * (1 + 3*pieces_.size());
    file_contents *out = new(alloc->alloc_content_data(len)) file_contents(pieces_.size());
    if (out == 0)
        return 0;
    for (int i = 0; i < pieces_.size(); i++) {
        const unsigned char *p = reinterpret_cast<const unsigned char*>
            (pieces_[i].data());
        chunk *chunk = alloc->chunk_from_string(p);
        out->buf_[3*i]     = chunk->id;
        out->buf_[3*i + 1] = p - chunk->data;
        out->buf_[3*i + 2] = pieces_[i].size();
    }
    return out;
}
