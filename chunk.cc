#include "chunk.h"
#include "radix_sort.h"

#include <re2/re2.h>
#include <gflags/gflags.h>

#include <limits>

using re2::StringPiece;

DECLARE_bool(index);

DEFINE_int32(chunk_power, 24, "Size of search chunks, as a power of two");

static bool validate_chunk_power(const char* flagname, int32_t value) {
    if (value > 10 && value < 30) {
        kChunkSize = (1 << value);
        kChunkSpace = kChunkSize - sizeof(chunk);
        return true;
    }
    return false;
}

static const bool dummy = google::RegisterFlagValidator(&FLAGS_chunk_power,
                                                        validate_chunk_power);

size_t kChunkSize = (1 << 24);
size_t kChunkSpace(kChunkSize - sizeof(chunk));

class radix_sorter {
public:
    radix_sorter(chunk *chunk) : chunk_(chunk) {
        lengths = new uint32_t[chunk_->size];
        for (int i = 0; i < chunk_->size; i ++)
            lengths[i] = static_cast<unsigned char*>
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
            unsigned char *l = &sort.chunk_->data[lhs];
            unsigned char *r = &sort.chunk_->data[rhs];
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
    int l = (unsigned char*)line.data() - data;
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
    cf.files.push_front(sf);
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
    msd_radix_sort(chunk_->suffixes, chunk_->suffixes + chunk_->size, 0,
                   idx, cmp);
    assert(is_sorted(chunk_->suffixes, chunk_->suffixes + chunk_->size, cmp));
}

void chunk::finalize() {
    if (FLAGS_index) {
        suffixes = new uint32_t[size];
        for (int i = 0; i < size; i++)
            suffixes[i] = i;
        radix_sorter sorter(this);
        sorter.sort();
    }
}

void chunk::finalize_files() {
    sort(files.begin(), files.end());

    vector<chunk_file>::iterator out, in;
    out = in = files.begin();
    while (in != files.end()) {
        *out = *in;
        ++in;
        while (in != files.end() &&
               out->left == in->left &&
               out->right == in->right) {
            out->files.push_back(in->files.front());
            ++in;
        }
        ++out;
    }
    files.resize(out - files.begin());
    build_tree();
}

void chunk::build_tree() {
    assert(is_sorted(files.begin(), files.end()));
    cf_root = build_tree(0, files.size());
}

chunk_file_node *chunk::build_tree(int left, int right) {
    if (right == left)
        return 0;
    int mid = (left + right) / 2;
    chunk_file_node *node = new chunk_file_node;

    node->chunk = &files[mid];
    node->left  = build_tree(left, mid);
    node->right = build_tree(mid + 1, right);
    node->right_limit = node->chunk->right;
    if (node->left && node->left->right_limit > node->right_limit)
        node->right_limit = node->left->right_limit;
    if (node->right && node->right->right_limit > node->right_limit)
        node->right_limit = node->right->right_limit;
    assert(!node->left  || *(node->left->chunk) < *(node->chunk));
    assert(!node->right || *(node->chunk) < *(node->right->chunk));
    return node;
}
