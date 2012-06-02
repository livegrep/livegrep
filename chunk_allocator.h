#include <list>
#include <map>
#include "thread_pool.h"

using namespace std;
struct chunk;
class code_searcher;

class chunk_allocator {
public:
    chunk_allocator();
    unsigned char *alloc(size_t len);

    list<chunk*>::iterator begin () {
        return chunks_.begin();
    }

    list<chunk*>::iterator end () {
        return chunks_.end();
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
    void replace_data(chunk *chunk, unsigned char *new_data);

protected:

    struct finalizer {
        bool operator()(chunk *chunk);
    };

    virtual chunk *alloc_chunk();
    void new_chunk();

    list<chunk*> chunks_;
    chunk *current_;
    finalizer finalizer_;
    thread_pool<chunk*, finalizer> *finalize_pool_;
    map<const unsigned char*, chunk*> by_data_;
};

// dump_load.cc
chunk_allocator *make_dump_allocator(code_searcher *search, const char *path);
