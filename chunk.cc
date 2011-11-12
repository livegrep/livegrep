#include "chunk.h"
#include "radix_sort.h"

#include <re2/re2.h>
#include <gflags/gflags.h>

using re2::StringPiece;

DECLARE_bool(index);

class radix_sorter {
public:
    radix_sorter(chunk *chunk) : chunk_(chunk) {
        lengths = new uint32_t[chunk_->size];
        for (int i = 0; i < chunk_->size; i ++)
            lengths[i] = static_cast<char*>
                (memchr(&chunk_->data[i], '\n', chunk_->size - i)) -
                (chunk_->data + i);
    }

    ~radix_sorter() {
        delete[] lengths;
    }

    void sort();

    struct cmp_suffix {
        radix_sorter &sort;
        cmp_suffix(radix_sorter &s) : sort(s) {}
        bool operator()(uint32_t lhs, uint32_t rhs) {
            char *l = &sort.chunk_->data[lhs];
            char *r = &sort.chunk_->data[rhs];
            unsigned ll = sort.lengths[lhs];
            unsigned rl = sort.lengths[rhs];
            int cmp = memcmp(l, r, min(ll, rl));
            if (cmp < 0)
                return true;
            if (cmp > 0)
                return false;
            return ll < rl;
        }
    };

    struct indexer {
        radix_sorter &sort;
        indexer(radix_sorter &s) : sort(s) {}
        unsigned operator()(uint32_t off, int i) {
            return sort.index(off, i);
        }
    };

private:
    unsigned index(uint32_t off, int i) {
        if (i >= lengths[off]) return 0;
        return (unsigned)(unsigned char)chunk_->data[off + i];
    }

    chunk *chunk_;
    unsigned *lengths;

    radix_sorter(const radix_sorter&);
    radix_sorter operator=(const radix_sorter&);
};

void chunk::add_chunk_file(search_file *sf, const StringPiece& line)
{
    int l = line.data() - data;
    int r = l + line.size();
    chunk_file *f = NULL;
    int min_dist = numeric_limits<int>::max(), dist;
    for (vector<chunk_file>::iterator it = cur_file.begin();
         it != cur_file.end(); it ++) {
        if (l <= it->left)
            dist = max(0, it->left - r);
        else if (r >= it->right)
            dist = max(0, l - it->right);
        else
            dist = 0;
        assert(dist == 0 || r < it->left || l > it->right);
        if (dist < min_dist) {
            min_dist = dist;
            f = &(*it);
        }
    }
    if (f && min_dist < kMaxGap) {
        f->expand(l, r);
        return;
    }
    chunk_files++;
    cur_file.push_back(chunk_file());
    chunk_file& cf = cur_file.back();
    cf.file = sf;
    cf.left = l;
    cf.right = r;
}

void chunk::finish_file() {
    int right = -1;
    sort(cur_file.begin(), cur_file.end());
    for (vector<chunk_file>::iterator it = cur_file.begin();
         it != cur_file.end(); it ++) {
        assert(right < it->left);
        right = max(right, it->right);
    }
    files.insert(files.end(), cur_file.begin(), cur_file.end());
    cur_file.clear();
}

int chunk::chunk_files = 0;

void radix_sorter::sort() {
    cmp_suffix cmp(*this);
    indexer idx(*this);
    radix_sort(chunk_->suffixes, chunk_->suffixes + chunk_->size, 0,
               idx, cmp);
    assert(is_sorted(chunk_->suffixes, chunk_->suffixes + chunk_->size, cmp));
}


void chunk::finalize() {
    if (FLAGS_index) {
        suffixes = new uint32_t[size];
        for (int i = 0; i < size; i++)
            suffixes[i] = i;
        radix_sorter sort(this);
        sort.sort();
    }
}
