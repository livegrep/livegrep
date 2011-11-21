#include "codesearch.h"
#include "chunk.h"
#include "chunk_allocator.h"

const uint32_t kIndexMagic   = 0xc0d35eac;
const uint32_t kIndexVersion = 2;

struct index_header {
    uint32_t magic;
    uint32_t version;
    uint32_t chunk_size;
    uint32_t nrefs;
    uint32_t nfiles;
    uint32_t nchunks;
} __attribute__((packed));


struct chunk_header {
    uint32_t size;
    uint32_t nfiles;
} __attribute__((packed));

static void dump_int32(ostream& stream, uint32_t i) {
    stream.write(reinterpret_cast<char*>(&i), sizeof i);
}

static void dump_string(ostream& stream, const char *str) {
    uint32_t len = strlen(str);
    dump_int32(stream, len);
    stream.write(str, len);
}

void code_searcher::dump_file(ostream& stream, search_file *sf) {
    /* (str path, int ref, oid id) */
    dump_string(stream, sf->path.c_str());
    dump_int32(stream, find(refs_.begin(), refs_.end(), sf->ref) - refs_.begin());
    stream.write(reinterpret_cast<char*>(&sf->oid), sizeof sf->oid);
}

void dump_chunk_file(ostream& stream, chunk_file *cf) {
    dump_int32(stream, cf->file->no);
    dump_int32(stream, cf->left);
    dump_int32(stream, cf->right);
}

void code_searcher::dump_chunk(ostream& stream, chunk *chunk) {
    chunk_header hdr = { uint32_t(chunk->size), uint32_t(chunk->files.size()) };
    stream.write(reinterpret_cast<char*>(&hdr), sizeof hdr);
    for (vector<chunk_file>::iterator it = chunk->files.begin();
         it != chunk->files.end(); it ++)
        dump_chunk_file(stream, &(*it));
    stream.write(reinterpret_cast<char*>(chunk->data), chunk->size);
    stream.write(reinterpret_cast<char*>(chunk->suffixes),
                 sizeof(uint32_t) * chunk->size);
}

void code_searcher::dump_index(const string& path) {
    assert(finalized_);
    ofstream stream(path.c_str());
    index_header hdr;
    hdr.magic   = kIndexMagic;
    hdr.version = kIndexVersion;
    hdr.chunk_size = kChunkSize;
    hdr.nrefs   = refs_.size();
    hdr.nfiles  = files_.size();
    hdr.nchunks = alloc_->size();

    stream.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    for (vector<const char*>::iterator it = refs_.begin();
         it != refs_.end(); ++it) {
        dump_string(stream, *it);
    }

    for (vector<search_file*>::iterator it = files_.begin();
         it != files_.end(); ++it) {
        dump_file(stream, *it);
    }

    for (list<chunk*>::iterator it = alloc_->begin();
         it != alloc_->end(); ++it) {
        dump_chunk(stream, *it);
    }
}

uint32_t load_int32(istream& stream) {
    uint32_t out;
    stream.read(reinterpret_cast<char*>(&out), sizeof out);
    return out;
}

char *load_string(istream& stream) {
    uint32_t len = load_int32(stream);
    char *buf = new char[len + 1];
    stream.read(buf, len);
    buf[len] = 0;
    return buf;
}

search_file *code_searcher::load_file(istream& stream) {
    search_file *sf = new search_file;
    char *str = load_string(stream);
    sf->path = str;
    delete[] str;
    sf->ref = refs_[load_int32(stream)];
    stream.read(reinterpret_cast<char*>(&sf->oid), sizeof(sf->oid));
    sf->no = files_.size();
    return sf;
}

void code_searcher::load_chunk_file(istream& stream, chunk_file *cf) {
    cf->file = files_[load_int32(stream)];
    cf->left = load_int32(stream);
    cf->right = load_int32(stream);
}

void code_searcher::load_chunk(istream& stream, chunk *chunk) {
    chunk_header hdr;
    stream.read(reinterpret_cast<char*>(&hdr), sizeof hdr);
    assert(hdr.size <= kChunkSpace);
    chunk->size = hdr.size;
    for (int i = 0; i < hdr.nfiles; i++) {
        chunk->files.push_back(chunk_file());
        load_chunk_file(stream, &chunk->files.back());
    }
    stream.read(reinterpret_cast<char*>(chunk->data), chunk->size);
    chunk->suffixes = new uint32_t[chunk->size];
    stream.read(reinterpret_cast<char*>(chunk->suffixes),
                sizeof(uint32_t) * chunk->size);
}

void code_searcher::load_index(const string& path) {
    assert(!finalized_);
    assert(!refs_.size());

    ifstream stream(path.c_str());
    index_header hdr;
    stream.read(reinterpret_cast<char*>(&hdr), sizeof hdr);
    assert(!stream.fail());
    assert(hdr.magic == kIndexMagic);
    assert(hdr.version == kIndexVersion);
    assert(hdr.chunk_size == kChunkSize);

    for (int i = 0; i < hdr.nrefs; i++) {
        refs_.push_back(load_string(stream));
    }

    for (int i = 0; i < hdr.nfiles; i++) {
        files_.push_back(load_file(stream));
    }

    for (int i = 0; i < hdr.nchunks; i++) {
        load_chunk(stream, alloc_->current_chunk());
        if (i != hdr.nchunks - 1)
            alloc_->skip_chunk();
    }

    finalized_ = true;
}
