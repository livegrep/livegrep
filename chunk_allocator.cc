#include "chunk_allocator.h"
#include "chunk.h"

bool chunk_allocator::finalizer::operator()(chunk *chunk) {
    if (!chunk)
        return true;
    chunk->finalize();
    return false;
}

chunk_allocator::chunk_allocator()  :
    current_(0), finalizer_() {
    new_chunk();
    finalize_pool_ = new thread_pool<chunk*, finalizer>(4, finalizer_);
}

char *chunk_allocator::alloc(size_t len) {
    assert(len < kChunkSpace);
    if ((current_->size + len) > kChunkSpace)
        new_chunk();
    char *out = current_->data + current_->size;
    current_->size += len;
    return out;
}

static chunk *alloc_chunk() {
    void *p;
    if (posix_memalign(&p, kChunkSize, kChunkSize) != 0)
        return NULL;
    return new(p) chunk;
};

void chunk_allocator::new_chunk()  {
    if (current_)
        finalize_pool_->queue(current_);
    current_ = alloc_chunk();
    chunks_.push_back(current_);
}

void chunk_allocator::finalize()  {
    finalize_pool_->queue(current_);
    for (int i = 0; i < 4; i++)
        finalize_pool_->queue(NULL);
    delete finalize_pool_;
    finalize_pool_ = NULL;
}
