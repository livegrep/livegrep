#include "chunk_allocator.h"
#include "chunk.h"

#include <gflags/gflags.h>

#include <sys/mman.h>

DECLARE_int32(threads);
DECLARE_bool(index);

bool chunk_allocator::finalizer::operator()(chunk *chunk) {
    if (!chunk)
        return true;
    chunk->finalize();
    return false;
}

chunk_allocator::chunk_allocator()  :
    current_(0), finalizer_(), finalize_pool_(0) {
    //    new_chunk();
}

chunk_allocator::~chunk_allocator() {
}

void chunk_allocator::cleanup() {
    for (auto c = begin(); c != end(); ++ c)
        free_chunk(*c);
}

unsigned char *chunk_allocator::alloc(size_t len) {
    assert(len < kChunkSize);
    if (current_ == 0 || (current_->size + len) > kChunkSize)
        new_chunk();
    unsigned char *out = current_->data + current_->size;
    current_->size += len;
    return out;
}

void chunk_allocator::finish_chunk()  {
    if (current_) {
        if (!finalize_pool_) {
            finalize_pool_ = new thread_pool<chunk*, finalizer>(FLAGS_threads, finalizer_);
        }
        finalize_pool_->queue(current_);
    }
}

void chunk_allocator::new_chunk()  {
    finish_chunk();
    current_ = alloc_chunk();
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
}

void chunk_allocator::skip_chunk() {
    current_ = 0;
    new_chunk();
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
public:
    virtual chunk *alloc_chunk() {
        unsigned char *buf = new unsigned char[kChunkSize];
        uint32_t *idx = FLAGS_index ? new uint32_t[kChunkSize] : 0;
        return new chunk(buf, idx);
    }

    virtual void free_chunk(chunk *chunk) {
        delete[] chunk->data;
        delete[] chunk->suffixes;
        delete chunk;
    }
};

chunk_allocator *make_mem_allocator() {
    return new mem_allocator();
}
