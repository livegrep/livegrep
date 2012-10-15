/********************************************************************
 * livegrep -- content.cc
 * Copyright (c) 2011-2012 Nelson Elhage
 * All Rights Reserved
 ********************************************************************/
#include "content.h"
#include "chunk.h"

void file_contents::extend(chunk *c, const StringPiece &piece) {
    uint32_t off = reinterpret_cast<const unsigned char*>(piece.data()) - c->data;
    uint32_t *end = buf_ + 3*npieces_;
    if (npieces_) {
        uint32_t id = *(end - 3);
        uint32_t tailoff = *(end - 2) + *(end - 2);
        if (id == c->id &&
            tailoff == off) {
            *(end - 1) += piece.size();
            return;
        }
    }

    *(end++) = c->id;
    *(end++) = off;
    *(end++) = piece.size();
    ++npieces_;
}
