/********************************************************************
 * livegrep -- content.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_CONTENT_H
#define CODESEARCH_CONTENT_H

#include <vector>
#include "re2/re2.h"

#include "src/chunk.h"
#include "src/chunk_allocator.h"

using re2::StringPiece;
using std::vector;


class file_contents {
public:
    struct piece {
        uint32_t chunk;
        uint32_t off;
        uint32_t len;
    } __attribute__((packed));

    template <class T>
    class proxy {
        T obj;
    public:
        proxy(T obj) : obj(obj) {}
        T *operator->() {
            return &obj;
        }
    };
    class iterator {
    public:
        const StringPiece operator*() {
            return StringPiece(reinterpret_cast<char*>(alloc_->at(it_->chunk)->data + it_->off),
                               it_->len);
        }

        proxy<StringPiece> operator->() {
            return proxy<StringPiece>(this->operator*());
        }

        iterator &operator++() {
            it_++;
            return *this;
        }

        iterator &operator--() {
            it_--;
            return *this;
        }

        bool operator==(const iterator &rhs) {
            return it_ == rhs.it_;
        }
        bool operator!=(const iterator &rhs) {
            return !(*this == rhs);
        }
    protected:
        iterator(chunk_allocator *alloc, piece *it)
            : alloc_(alloc), it_(it) {}

        chunk_allocator *alloc_;
        piece *it_;

        friend class file_contents;
    };

    file_contents(uint32_t npieces) : npieces_(npieces) { }

    iterator begin(chunk_allocator *alloc) {
        return iterator(alloc, pieces_);
    }

    iterator end(chunk_allocator *alloc) {
        return iterator(alloc, pieces_ + npieces_);
    }

    piece *begin() {
        return pieces_;
    }

    piece *end() {
        return pieces_ + npieces_;
    }

    size_t size() {
        return npieces_;
    }

    friend class codesearch_index;
    friend class load_allocator;
    friend class file_contents_builder;

protected:
    file_contents() {}

    uint32_t npieces_;
    piece pieces_[];
};

class file_contents_builder {
public:
    void extend(chunk *chunk, const StringPiece &piece);
    file_contents *build(chunk_allocator *alloc);
protected:
    vector <StringPiece> pieces_;
};

#endif
