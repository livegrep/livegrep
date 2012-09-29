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
        iterator(chunk_allocator *alloc, vector<uint32_t>::iterator it)
            : alloc_(alloc), it_(it) {}

        chunk_allocator *alloc_;
        vector<uint32_t>::iterator it_;

        friend class file_contents;
    };

    iterator begin(chunk_allocator *alloc) {
        return iterator(alloc, pieces.begin());
    }

    iterator end(chunk_allocator *alloc) {
        return iterator(alloc, pieces.end());
    }

    void extend(chunk *chunk, const StringPiece &piece);

    friend class codesearch_index;

protected:
    vector<uint32_t> pieces;
};

#endif
