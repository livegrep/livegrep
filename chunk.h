#include <assert.h>
#include <string.h>

#include <vector>
#include <string>
#include <algorithm>

#include <stdint.h>

struct search_file;
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

extern size_t kChunkSize;
const size_t kMaxGap       = 1 << 10;
#define CHUNK_MAGIC 0xC407FADE

struct chunk {
    static int chunk_files;
    int size;
    unsigned magic;
    vector<chunk_file> files;
    vector<chunk_file> cur_file;
    uint32_t *suffixes;
    unsigned char data[0];

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
            const unsigned char *l = &chunk_->data[lhs];
            const unsigned char *r = &chunk_->data[rhs];
            const unsigned char *le = static_cast<const unsigned char*>
                (memchr(l, '\n', chunk_->size - lhs));
            const unsigned char *re = static_cast<const unsigned char*>
                (memchr(r, '\n', chunk_->size - rhs));
            assert(le);
            assert(re);
            return memcmp(l, r, min(le - l, re - r)) < 0;
        }

        bool operator()(uint32_t lhs, const string& rhs) {
            return cmp(lhs, rhs) < 0;
        }

        bool operator()(const string& lhs, uint32_t rhs) {
            return cmp(rhs, lhs) > 0;
        }

    private:
        int cmp(uint32_t lhs, const string& rhs) {
            const unsigned char *l = &chunk_->data[lhs];
            const unsigned char *le = static_cast<const unsigned char*>
                (memchr(l, '\n', chunk_->size - lhs));
            size_t lhs_len = le - l;
            int cmp = memcmp(l, rhs.c_str(), min(lhs_len, rhs.size()));
            if (cmp == 0)
                return lhs_len < rhs.size() ? -1 : 0;
            return cmp;
        }
    };

private:
    chunk(const chunk&);
    chunk operator=(const chunk&);
};

extern size_t kChunkSpace;
