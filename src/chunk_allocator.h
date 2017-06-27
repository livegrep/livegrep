/********************************************************************
 * livegrep -- chunk_allocator.h
 * Copyright (c) 2011-2013 Nelson Elhage
 *
 * This program is free software. You may use, redistribute, and/or
 * modify it under the terms listed in the COPYING file.
 ********************************************************************/
#ifndef CODESEARCH_CHUNK_ALLOCATOR_H
#define CODESEARCH_CHUNK_ALLOCATOR_H

#include <vector>
#include <map>
#include <string>
#include <thread>
#include <assert.h>

#include "src/lib/thread_queue.h"

using namespace std;
struct chunk;
class code_searcher;

struct buffer {
    uint8_t *data;
    uint8_t *end;
};

class chunk_allocator {
public:
    chunk_allocator();
    virtual ~chunk_allocator();
    void cleanup();

    void set_chunk_size(size_t size);
    size_t chunk_size() {
        return chunk_size_;
    }

    unsigned char *alloc(size_t len);
    uint8_t *alloc_content_data(size_t len);

    vector<chunk*>::iterator begin () {
        return chunks_.begin();
    }

    vector<chunk*>::iterator end () {
        return chunks_.end();
    }

    vector<buffer>::const_iterator begin_content() {
        return content_chunks_.begin();
    }

    vector<buffer>::const_iterator end_content() {
        return content_chunks_.end();
    }

    chunk *at(size_t i) {
        assert(i < chunks_.size());
        return chunks_[i];
    }

    size_t size () {
        return chunks_.size();
    }

    chunk *current_chunk() {
        return current_;
    }

    void skip_chunk();
    virtual void finalize();

    chunk *chunk_from_string(const unsigned char *p);

    virtual void drop_caches();
protected:
    static void finalize_worker(chunk_allocator *);

    virtual chunk *alloc_chunk() = 0;
    virtual void free_chunk(chunk *chunk) = 0;
    virtual buffer alloc_content_chunk() = 0;
    void finish_chunk();
    void new_chunk();

    size_t chunk_size_;
    vector<chunk*> chunks_;
    vector<buffer> content_chunks_;

    // Subsequent fields are transient (only used during index creation).

    // Tracks how much of the current content chunk has been allocated by
    // alloc_content_data().
    uint8_t *content_finger_;

    // Points to the chunk currently being filled (which is also chunks_.back()).
    chunk *current_;

    // Machinery to finalize chunks (i.e. build the suffix array from the data)
    // in the background.
    thread_queue<chunk*> finalize_queue_;
    vector<std::thread> threads_;

    // Used by chunk_from_string() to efficiently find the chunk containing an
    // already-indexed line of code.
    map<const unsigned char*, chunk*> by_data_;
};

const size_t kContentChunkSize = (1UL << 22);

#endif
