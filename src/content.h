/********************************************************************
 * livegrep -- content.h
 * Copyright (c) 2011-2012 Nelson Elhage
 * All Rights Reserved
 ********************************************************************/
#ifndef CODESEARCH_CONTENT_H
#define CODESEARCH_CONTENT_H

#include <vector>
#include <re2/re2.h>

#include "chunk.h"
#include "chunk_allocator.h"

using re2::StringPiece;
using std::vector;

class file_contents {
public:
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
            return StringPiece(reinterpret_cast<char*>(alloc_->at(*it_)->data + *(it_+1)),
                               *(it_+2));
        }

        proxy<StringPiece> operator->() {
            return proxy<StringPiece>(this->operator*());
        }

        iterator &operator++() {
            it_ += 3;
            return *this;
        }

        iterator &operator--() {
            it_ -= 3;
            return *this;
        }

        bool operator==(const iterator &rhs) {
            return it_ == rhs.it_;
        }
        bool operator!=(const iterator &rhs) {
            return !(*this == rhs);
        }
    protected:
        iterator(chunk_allocator *alloc, uint32_t *it)
            : alloc_(alloc), it_(it) {}

        chunk_allocator *alloc_;
        uint32_t *it_;

        friend class file_contents;
    };

    file_contents(uint32_t npieces) : npieces_(npieces) { }

    iterator begin(chunk_allocator *alloc) {
        return iterator(alloc, buf_);
    }

    iterator end(chunk_allocator *alloc) {
        return iterator(alloc, buf_ + 3*npieces_);
    }

    uint32_t *begin() {
        return buf_;
    }

    uint32_t *end() {
        return buf_ + 3*npieces_;
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
    uint32_t buf_[];
};

class file_contents_builder {
public:
    void extend(chunk *chunk, const StringPiece &piece);
    file_contents *build(chunk_allocator *alloc);
protected:
    vector <StringPiece> pieces_;
};

#endif
