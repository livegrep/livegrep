#include <list>

using namespace std;
class chunk;

class chunk_allocator {
public:
    chunk_allocator() : current_(0) {
        new_chunk();
    }

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
    void new_chunk();

    list<chunk*> chunks_;
    chunk *current_;
};
