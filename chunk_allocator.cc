#include "chunk_allocator.h"
#include "chunk.h"

#include <gflags/gflags.h>

DECLARE_int32(threads);

bool chunk_allocator::finalizer::operator()(chunk *chunk) {
    if (!chunk)
        return true;
    chunk->finalize();
    return false;
}

chunk_allocator::chunk_allocator()  :
    current_(0), finalizer_(), finalize_pool_(0) {
    new_chunk();
}

unsigned char *chunk_allocator::alloc(size_t len) {
    assert(len < kChunkSpace);
    if ((current_->size + len) > kChunkSpace)
        new_chunk();
    unsigned char *out = current_->data + current_->size;
    current_->size += len;
    return out;
}

static chunk *alloc_chunk() {
    return new chunk;
};

void chunk_allocator::new_chunk()  {
    if (current_) {
        if (!finalize_pool_) {
            finalize_pool_ = new thread_pool<chunk*, finalizer>(FLAGS_threads, finalizer_);
        }
        finalize_pool_->queue(current_);
    }
    current_ = alloc_chunk();
    chunks_.push_back(current_);
}

void chunk_allocator::finalize()  {
    if (!finalize_pool_)
        return;
    finalize_pool_->queue(current_);
    for (int i = 0; i < FLAGS_threads; i++)
        finalize_pool_->queue(NULL);
    delete finalize_pool_;
    finalize_pool_ = NULL;
    for (list<chunk*>::iterator it = begin(); it != end(); ++it)
        (*it)->finalize_files();
}

void chunk_allocator::skip_chunk() {
    current_ = 0;
    new_chunk();
}
