/********************************************************************
 * livegrep -- chunk_allocator.cc
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#include "chunk_allocator.h"
#include "chunk.h"

#include <gflags/gflags.h>

#include <sys/mman.h>

DECLARE_int32(threads);
DECLARE_bool(index);
DEFINE_int32(chunk_power, 24, "Size of search chunks, as a power of two");
size_t kChunkSize = (1 << 24);

static bool validate_chunk_power(const char* flagname, int32_t value) {
    if (value > 10 && value < 30) {
        kChunkSize = (1 << value);
        return true;
    }
    return false;
}

static const bool dummy = google::RegisterFlagValidator(&FLAGS_chunk_power,
                                                        validate_chunk_power);

bool chunk_allocator::finalizer::operator()(chunk *chunk) {
    if (!chunk)
        return true;
    // chunk->finalize();
    return false;
}

chunk_allocator::chunk_allocator()  :
    chunk_size_(kChunkSize), content_finger_(0), current_(0),
    finalizer_(), finalize_pool_(0) {
}

chunk_allocator::~chunk_allocator() {
}

void chunk_allocator::set_chunk_size(size_t size) {
    assert(current_ == 0);
    assert(!chunks_.size());
    chunk_size_ = size;
}

void chunk_allocator::cleanup() {
    for (auto c = begin(); c != end(); ++ c)
        free_chunk(*c);
}

unsigned char *chunk_allocator::alloc(size_t len) {
    assert(len < chunk_size_);
    if (current_ == 0 || (current_->size + len) > chunk_size_)
        new_chunk();
    unsigned char *out = current_->data + current_->size;
    current_->size += len;
    return out;
}

uint8_t *chunk_allocator::alloc_content_data(size_t len) {
    if (len >= kContentChunkSize)
        return 0;
    if (content_finger_ == 0 || (content_finger_ + len > content_chunks_.back().end)) {
        if (content_finger_)
            content_chunks_.back().end = content_finger_;
        content_chunks_.push_back(alloc_content_chunk());
        content_finger_ = content_chunks_.back().data;
    }
    uint8_t *out = content_finger_;
    content_finger_ += len;
    assert(content_finger_ > content_chunks_.back().data &&
           content_finger_ <= content_chunks_.back().end);
    return out;
}

btree_node *chunk_allocator::alloc_node() {
    return 0;
}

void chunk_allocator::finish_chunk()  {
    if (current_) {
        if (!finalize_pool_) {
            finalize_pool_ = new thread_pool<chunk*, finalizer>(FLAGS_threads, finalizer_);
        }
        // finalize_pool_->queue(current_);
        current_->finalize(this);
    }
}

void chunk_allocator::new_chunk()  {
    finish_chunk();
    current_ = alloc_chunk();
    madvise(current_->data, chunk_size_, MADV_RANDOM);
    current_->id = chunks_.size();
    by_data_[current_->data] = current_;
    chunks_.push_back(current_);
}

void chunk_allocator::finalize()  {
    if (!current_)
        return;
    finish_chunk();
    for (int i = 0; i < FLAGS_threads; i++)
        finalize_pool_->queue(NULL);
    delete finalize_pool_;
    finalize_pool_ = NULL;
    for (auto it = begin(); it != end(); ++it)
        (*it)->finalize_files();
    if (content_finger_)
        content_chunks_.back().end = content_finger_;
}

void chunk_allocator::skip_chunk() {
    current_ = 0;
    new_chunk();
}

void chunk_allocator::drop_caches() {
}

chunk *chunk_allocator::chunk_from_string(const unsigned char *p) {
    auto it = by_data_.lower_bound(p);
    if (it == by_data_.end() || it->first != p) {
        assert(it != by_data_.begin());
        --it;
    }
    assert(it->first <= p && p <= it->first + it->second->size);
    return it->second;
}

class mem_allocator : public chunk_allocator {
protected:
    btree_node *root_;
public:
    mem_allocator() {
        root_ = alloc_node();
        prepare_root(root_);
    }

    virtual chunk *alloc_chunk() {
        unsigned char *buf = new unsigned char[chunk_size_];
        return new chunk(buf);
    }

    virtual buffer alloc_content_chunk() {
        uint8_t *buf = new uint8_t[kContentChunkSize];
        return (buffer){ buf, buf + kContentChunkSize };
    }

    virtual void free_chunk(chunk *chunk) {
        delete[] chunk->data;
        delete chunk;
    }

    virtual btree_node *alloc_node() {
        return new btree_node();
    }

    virtual btree_node *btree_root() {
        return root_;
    }
    virtual void btree_set_root(btree_node *node) {
        root_ = node;
    }
};

chunk_allocator *make_mem_allocator() {
    return new mem_allocator();
}
