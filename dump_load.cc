#include "codesearch.h"
#include "chunk.h"
#include "chunk_allocator.h"

#include <map>

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

const uint32_t kIndexMagic   = 0xc0d35eac;
const uint32_t kIndexVersion = 8;
const uint32_t kPageSize     = (1 << 12);

struct index_header {
    uint32_t magic;
    uint32_t version;
    uint32_t chunk_size;
    uint32_t pad;

    uint64_t metadata_off;
    uint64_t chunks_off;
} __attribute__((packed));

struct metadata_header {
    uint32_t nrefs;
    uint32_t nfiles;
    uint32_t nchunks;
} __attribute__((packed));

struct chunk_header {
    uint32_t size;
    uint32_t nfiles;
} __attribute__((packed));

class codesearch_index {
public:
    codesearch_index(code_searcher *cs, string path, bool dump) :
        cs_(cs),
        stream_(path.c_str(), dump ? (ios::out | ios::trunc) : ios::in),
        hdr_() {
        assert(!stream_.fail());
        fd_ = open(path.c_str(), dump ? (O_RDWR|O_APPEND) : O_RDONLY);
        assert(fd_ > 0);

        if (dump) {
            hdr_.magic      = kIndexMagic;
            hdr_.version    = kIndexVersion;
            hdr_.chunk_size = kChunkSize;
        } else {
            load(&hdr_);
        }
    }

    ~codesearch_index() {
        close(fd_);
    }

    void dump();
    void load();
protected:
    void dump_chunk_data();
    void dump_metadata();
    void dump_file(search_file *);
    void dump_file_contents(search_file *);
    void dump_chunk_file(chunk_file *cf);
    void dump_chunk(chunk *);
    void dump_chunk_data(chunk *);

    search_file *load_file();
    void load_file_contents(search_file *sf);
    void load_chunk(chunk *);

    void alignp(uint32_t align) {
        streampos pos = stream_.tellp();
        stream_.seekp((size_t(pos) + align - 1) & ~(align - 1));
    }

    void aligng(uint32_t align) {
        streampos pos = stream_.tellg();
        stream_.seekg((size_t(pos) + align - 1) & ~(align - 1));
    }

    template<class T>
    void dump(T *t) {
        stream_.write(reinterpret_cast<char*>(t), sizeof *t);
    }

    void dump_int32(uint32_t i) {
        dump(&i);
    }

    void dump_string(const char *str) {
        uint32_t len = strlen(str);
        dump_int32(len);
        stream_.write(str, len);
    }

    template<class T>
    void load(T *t) {
        stream_.read(reinterpret_cast<char*>(t), sizeof *t);
    }

    uint32_t load_int32() {
        uint32_t out;
        load(&out);
        return out;
    }

    char *load_string() {
        uint32_t len = load_int32();
        char *buf = new char[len + 1];
        stream_.read(buf, len);
        buf[len] = 0;
        return buf;
    }

    code_searcher *cs_;
    std::fstream stream_;
    int fd_;

    index_header hdr_;
    map<chunk*, int> chunk_ids_;
    vector<chunk*> chunks_;

    friend class dump_allocator;
    friend class load_allocator;
};

class dump_allocator : public chunk_allocator {
public:
    dump_allocator(code_searcher *cs, const char *path)
        : index_(cs, path, true){
        index_.dump(&index_.hdr_);
        index_.alignp(kPageSize);
        index_.hdr_.chunks_off = index_.stream_.tellp();
    }

    virtual chunk *alloc_chunk() {
        void *buf;

        size_t off = index_.stream_.tellp();
        assert(ftruncate(index_.fd_, off + 5*kChunkSize) == 0);
        buf = mmap(NULL, 5*kChunkSize, PROT_READ|PROT_WRITE, MAP_SHARED,
                   index_.fd_, off);
        assert(buf != MAP_FAILED);
        index_.stream_.seekp(5*kChunkSize, ios::cur);

        return new chunk(static_cast<unsigned char*>(buf),
                         reinterpret_cast<uint32_t*>
                         (static_cast<unsigned char*>(buf) + kChunkSize));
    }

    virtual void finalize() {
        chunk_allocator::finalize();
        index_.dump_metadata();
        index_.stream_.seekp(0);
        index_.dump(&index_.hdr_);
        index_.stream_.close();
    }

    virtual void free_chunk(chunk *chunk) {
        munmap(chunk->data, 5*kChunkSize);
        delete chunk;
    }
protected:
    codesearch_index index_;
};

class load_allocator : public chunk_allocator {
public:
    load_allocator(codesearch_index *index)
        : index_(index){
        off_ = index_->hdr_.chunks_off;
    }

    virtual chunk *alloc_chunk() {
        void *buf;
        buf = mmap(NULL, 5*kChunkSize, PROT_READ, MAP_SHARED,
                   index_->fd_, off_);
        assert(buf != MAP_FAILED);
        off_ += 5*kChunkSize;

        return new chunk(static_cast<unsigned char*>(buf),
                         reinterpret_cast<uint32_t*>
                         (static_cast<unsigned char*>(buf) + kChunkSize));
    }

    virtual void free_chunk(chunk *chunk) {
        munmap(chunk->data, 5*kChunkSize);
        delete chunk;
    }
protected:
    codesearch_index *index_;
    size_t off_;
};

chunk_allocator *make_dump_allocator(code_searcher *search, const string& path) {
    return new dump_allocator(search, path.c_str());
}

void codesearch_index::dump_file(search_file *sf) {
    dump(&sf->oid);
    dump_int32(sf->paths.size());
    for (auto it = sf->paths.begin(); it != sf->paths.end(); ++it) {
        dump_int32(find(cs_->refs_.begin(), cs_->refs_.end(), it->ref) -
                   cs_->refs_.begin());
        dump_string(it->path.c_str());
    }
}

void codesearch_index::dump_chunk_file(chunk_file *cf) {
    dump_int32(cf->files.size());
    for (list<search_file*>::iterator it = cf->files.begin();
         it != cf->files.end(); ++it)
        dump_int32((*it)->no);

    dump_int32(cf->left);
    dump_int32(cf->right);
}

void codesearch_index::dump_chunk(chunk *chunk) {
    chunk_header hdr = { uint32_t(chunk->size), uint32_t(chunk->files.size()) };
    dump(&hdr);
    for (vector<chunk_file>::iterator it = chunk->files.begin();
         it != chunk->files.end(); it ++)
        dump_chunk_file(&(*it));
}

void codesearch_index::dump_chunk_data(chunk *chunk) {
    alignp(kPageSize);
    size_t off = stream_.tellp();
    assert(ftruncate(fd_, off + 5 * kChunkSize) == 0);
    stream_.write(reinterpret_cast<char*>(chunk->data), kChunkSize);
    stream_.write(reinterpret_cast<char*>(chunk->suffixes),
                  sizeof(uint32_t) * chunk->size);
    stream_.seekp(off + 5 * kChunkSize);
}


void codesearch_index::dump_file_contents(search_file *sf) {
    /* (int num, [chunkid, offset, len]) */
    dump_int32(sf->content.size());
    for (vector<StringPiece>::iterator it = sf->content.begin();
             it != sf->content.end(); ++it) {
        chunk *chunk = cs_->alloc_->chunk_from_string
            (reinterpret_cast<const unsigned char*>(it->data()));
        dump_int32(chunk_ids_[chunk]);
        dump_int32(reinterpret_cast<const unsigned char*>(it->data()) - chunk->data);
        dump_int32(it->size());
    }
}

void codesearch_index::dump_metadata() {
    hdr_.metadata_off = stream_.tellp();

    metadata_header meta;
    meta.nrefs   = cs_->refs_.size();
    meta.nfiles  = cs_->files_.size();
    meta.nchunks = cs_->alloc_->size();
    dump(&meta);

    for (vector<const char*>::iterator it = cs_->refs_.begin();
         it != cs_->refs_.end(); ++it) {
        dump_string(*it);
    }

    for (vector<search_file*>::iterator it = cs_->files_.begin();
         it != cs_->files_.end(); ++it) {
        dump_file(*it);
    }

    int i = 0;

    for (list<chunk*>::iterator it = cs_->alloc_->begin();
         it != cs_->alloc_->end(); ++it) {
        dump_chunk(*it);
        chunk_ids_[*it] = i++;
    }

    for (vector<search_file*>::iterator it = cs_->files_.begin();
         it != cs_->files_.end(); ++it) {
        dump_file_contents(*it);
    }
}

void codesearch_index::dump_chunk_data() {
    alignp(kPageSize);
    hdr_.chunks_off = stream_.tellp();
    for (list<chunk*>::iterator it = cs_->alloc_->begin();
         it != cs_->alloc_->end(); ++it) {
        dump_chunk_data(*it);
    }
}

void codesearch_index::dump() {
    assert(cs_->finalized_);

    dump(&hdr_);

    dump_chunk_data();
    dump_metadata();

    stream_.seekp(0);
    dump(&hdr_);
}

search_file *codesearch_index::load_file() {
    search_file *sf = new search_file;
    load(&sf->oid);
    sf->paths.resize(load_int32());
    for (auto it = sf->paths.begin(); it != sf->paths.end(); ++it) {
        it->ref = cs_->refs_[load_int32()];
        char *str = load_string();
        it->path = str;
        delete[] str;
    }
    sf->no = cs_->files_.size();
    return sf;
}

void codesearch_index::load_chunk(chunk *chunk) {
    chunk_header hdr;
    load(&hdr);
    assert(hdr.size <= kChunkSpace);
    chunk->size = hdr.size;

    /*
      uint32_t buf[3*hdr.nfiles];
      stream.read(reinterpret_cast<char*>(buf), sizeof buf);
    */
    for (int i = 0; i < hdr.nfiles; i++) {
        chunk->files.push_back(chunk_file());
        chunk_file &cf = chunk->files.back();
        uint32_t nfiles = load_int32();
        for (int j = 0; j < nfiles; j++)
            cf.files.push_back(cs_->files_[load_int32()]);
        cf.left  = load_int32();
        cf.right = load_int32();
    }
}

void codesearch_index::load_file_contents(search_file *sf) {
    int npieces = load_int32();
    uint32_t buf[3*npieces];

    stream_.read(reinterpret_cast<char*>(buf), sizeof buf);
    sf->content.resize(npieces);

    for (int i = 0; i < npieces; i++) {
        chunk *chunk = chunks_[buf[3*i]];
        char *p = reinterpret_cast<char*>(chunk->data) + buf[3*i + 1];
        int len = buf[3*i + 2];
        sf->content[i] = StringPiece(p, len);
    }
}

void codesearch_index::load() {
    assert(!cs_->finalized_);
    assert(!cs_->refs_.size());

    assert(hdr_.magic == kIndexMagic);
    assert(hdr_.version == kIndexVersion);
    assert(hdr_.chunk_size == kChunkSize);
    assert(hdr_.metadata_off);
    assert(hdr_.chunks_off);

    stream_.seekg(hdr_.metadata_off);
    metadata_header meta;
    load(&meta);

    for (int i = 0; i < meta.nrefs; i++) {
        cs_->refs_.push_back(load_string());
    }

    for (int i = 0; i < meta.nfiles; i++) {
        cs_->files_.push_back(load_file());
    }

    cs_->alloc_->skip_chunk();
    for (int i = 0; i < meta.nchunks; i++) {
        load_chunk(cs_->alloc_->current_chunk());
        chunks_.push_back(cs_->alloc_->current_chunk());
        cs_->alloc_->current_chunk()->build_tree();
        if (i != meta.nchunks - 1)
            cs_->alloc_->skip_chunk();
    }

    for (int i = 0; i < meta.nfiles; i++) {
        load_file_contents(cs_->files_[i]);
    }

    cs_->finalized_ = true;
}

void code_searcher::dump_index(const string &path) {
    codesearch_index idx(this, path, true);
    idx.dump();
}

void code_searcher::load_index(const string &path) {
    codesearch_index idx(this, path, false);
    set_alloc(new load_allocator(&idx));
    idx.load();
}
