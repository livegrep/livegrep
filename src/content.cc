/********************************************************************
 * livegrep -- content.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "src/content.h"
#include "src/chunk.h"

void file_contents_builder::extend(chunk *c, const StringPiece &piece) {
    if (pieces_.size() && piece.size()) {
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
    unsigned char *mem = alloc->alloc_content_data(len);
    if (mem == nullptr) return nullptr;
    file_contents *out = new(mem) file_contents(pieces_.size());
    for (int i = 0; i < pieces_.size(); i++) {
        const unsigned char *p = reinterpret_cast<const unsigned char*>
            (pieces_[i].data());
        chunk *chunk = alloc->chunk_from_string(p);
        out->pieces_[i].chunk = chunk->id;
        out->pieces_[i].off   = p - chunk->data;
        out->pieces_[i].len   = pieces_[i].size();
    }
    return out;
}
