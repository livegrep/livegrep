#include <list>
#include "thread_pool.h"

using namespace std;
class chunk;

class chunk_allocator {
public:
    chunk_allocator();
    char *alloc(size_t len);

    list<chunk*>::iterator begin () {
        return chunks_.begin();
    }

    typename list<chunk*>::iterator end () {
        return chunks_.end();
    }

    chunk *current_chunk() {
        return current_;
    }

    void finalize();

protected:

    struct finalizer {
        bool operator()(chunk *chunk);
    };

    void new_chunk();

    list<chunk*> chunks_;
    chunk *current_;
    finalizer finalizer_;
    thread_pool<chunk*, finalizer> *finalize_pool_;
};
