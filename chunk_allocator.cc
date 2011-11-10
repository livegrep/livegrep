#include "chunk_allocator.h"
#include "chunk.h"

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
        current_->finalize();
    current_ = alloc_chunk();
    chunks_.push_back(current_);
}

void chunk_allocator::finalize()  {
    current_->finalize();
}
