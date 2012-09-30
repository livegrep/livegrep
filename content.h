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

    void extend(chunk *chunk, const StringPiece &piece);

    friend class codesearch_index;

protected:
    file_contents() {}

    uint32_t npieces_;
    uint32_t buf_[];
};

#endif
