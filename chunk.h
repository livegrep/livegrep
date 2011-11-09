#include <assert.h>
#include <string.h>

#include <vector>
#include <string>
#include <algorithm>

#include <stdint.h>

class search_file;
namespace re2 {
    class StringPiece;
}

using namespace std;
using re2::StringPiece;

struct chunk_file {
    search_file *file;
    int left;
    int right;
    void expand(int l, int r) {
        left  = min(left, l);
        right = max(right, r);
    }

    bool operator<(const chunk_file& rhs) const {
        return left < rhs.left;
    }
};

const size_t kChunkSize    = 1 << 20;
const size_t kMaxGap       = 1 << 10;
#define CHUNK_MAGIC 0xC407FADE

struct chunk {
    static int chunk_files;
    int size;
    unsigned magic;
    vector<chunk_file> files;
    vector<chunk_file> cur_file;
    uint32_t *suffixes;
    char data[0];

    chunk()
        : size(0), magic(CHUNK_MAGIC), files(), suffixes(0) {
    }

    void add_chunk_file(search_file *sf, const StringPiece& line);
    void finish_file();
    void finalize();

    static chunk *from_str(const char *p) {
        chunk *out = reinterpret_cast<chunk*>
            ((uintptr_t(p) - 1) & ~(kChunkSize - 1));
        assert(out->magic == CHUNK_MAGIC);
        return out;
    }

    struct lt_suffix {
        const chunk *chunk_;
        lt_suffix(const chunk *chunk) : chunk_(chunk) { }
        bool operator()(uint32_t lhs, uint32_t rhs) {
            const char *l = &chunk_->data[lhs];
            const char *r = &chunk_->data[rhs];
            const char *le = static_cast<const char*>
                (memchr(l, '\n', chunk_->size - lhs));
            const char *re = static_cast<const char*>
                (memchr(r, '\n', chunk_->size - rhs));
            assert(le);
            assert(re);
            return strncmp(l, r, min(le - l, re - r)) < 0;
        }

        bool operator()(uint32_t lhs, const string& rhs) {
            return cmp(lhs, rhs) < 0;
        }

        bool operator()(const string& lhs, uint32_t rhs) {
            return cmp(rhs, lhs) > 0;
        }

    private:
        int cmp(uint32_t lhs, const string& rhs) {
            const char *l = &chunk_->data[lhs];
            const char *le = static_cast<const char*>
                (memchr(l, '\n', chunk_->size - lhs));
            return strncmp(l, rhs.c_str(), min(le - l, long(rhs.size())));
        }
    };

private:
    chunk(const chunk&);
    chunk operator=(const chunk&);
};

const size_t kChunkSpace = kChunkSize - sizeof(chunk);
