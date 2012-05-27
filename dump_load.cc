#include "codesearch.h"
#include "chunk.h"
#include "chunk_allocator.h"

#include <map>

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

const uint32_t kIndexMagic   = 0xc0d35eac;
const uint32_t kIndexVersion = 7;
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

/*
struct chunks_header {
    
} __attribute__((packed));
*/

struct chunk_header {
    uint32_t size;
    uint32_t nfiles;
} __attribute__((packed));

namespace {
    void stream_alignp(ostream& stream, uint32_t align) {
        streampos pos = stream.tellp();
        stream.seekp((size_t(pos) + align) & ~(align - 1));
    }

    void stream_aligng(istream& stream, uint32_t align) {
        streampos pos = stream.tellg();
        stream.seekg((size_t(pos) + align) & ~(align - 1));
    }

    void dump_int32(ostream& stream, uint32_t i) {
        stream.write(reinterpret_cast<char*>(&i), sizeof i);
    }

    void dump_string(ostream& stream, const char *str) {
        uint32_t len = strlen(str);
        dump_int32(stream, len);
        stream.write(str, len);
    }
};

void code_searcher::dump_file(ostream& stream, search_file *sf) {
    stream.write(reinterpret_cast<char*>(&sf->oid), sizeof sf->oid);
    dump_int32(stream, sf->paths.size());
    for (auto it = sf->paths.begin(); it != sf->paths.end(); ++it) {
        dump_int32(stream, find(refs_.begin(), refs_.end(), it->ref) - refs_.begin());
        dump_string(stream, it->path.c_str());
    }
}

void dump_chunk_file(ostream& stream, chunk_file *cf) {
    dump_int32(stream, cf->files.size());
    for (list<search_file*>::iterator it = cf->files.begin();
         it != cf->files.end(); ++it)
        dump_int32(stream, (*it)->no);

    dump_int32(stream, cf->left);
    dump_int32(stream, cf->right);
}

void code_searcher::dump_chunk(ostream& stream, chunk *chunk) {
    chunk_header hdr = { uint32_t(chunk->size), uint32_t(chunk->files.size()) };
    stream.write(reinterpret_cast<char*>(&hdr), sizeof hdr);
    for (vector<chunk_file>::iterator it = chunk->files.begin();
         it != chunk->files.end(); it ++)
        dump_chunk_file(stream, &(*it));
}

void code_searcher::dump_chunk_data(ostream& stream, chunk *chunk) {
    stream_alignp(stream, kPageSize);
    stream.write(reinterpret_cast<char*>(chunk->data), chunk->size);
    stream_alignp(stream, kPageSize);
    stream.write(reinterpret_cast<char*>(chunk->suffixes),
                 sizeof(uint32_t) * chunk->size);
}


void code_searcher::dump_file_contents(ostream& stream,
                                       map<chunk*, int>& chunks,
                                       search_file *sf) {
    /* (int num, [chunkid, offset, len]) */
    dump_int32(stream, sf->content.size());
    for (vector<StringPiece>::iterator it = sf->content.begin();
             it != sf->content.end(); ++it) {
        chunk *chunk = chunk::from_str(reinterpret_cast<const unsigned char*>(it->data()));
        dump_int32(stream, chunks[chunk]);
        dump_int32(stream, reinterpret_cast<const unsigned char*>(it->data()) - chunk->data);
        dump_int32(stream, it->size());
    }
}


void code_searcher::dump_index(const string& path) {
    assert(finalized_);
    ofstream stream(path.c_str());
    index_header hdr = {};
    hdr.magic   = kIndexMagic;
    hdr.version = kIndexVersion;
    hdr.chunk_size = kChunkSize;

    metadata_header meta;
    meta.nrefs   = refs_.size();
    meta.nfiles  = files_.size();
    meta.nchunks = alloc_->size();

    stream.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    hdr.metadata_off = stream.tellp();
    stream.write(reinterpret_cast<const char*>(&meta), sizeof(meta));

    for (vector<const char*>::iterator it = refs_.begin();
         it != refs_.end(); ++it) {
        dump_string(stream, *it);
    }

    for (vector<search_file*>::iterator it = files_.begin();
         it != files_.end(); ++it) {
        dump_file(stream, *it);
    }

    map<chunk*, int> chunks;
    int i = 0;

    for (list<chunk*>::iterator it = alloc_->begin();
         it != alloc_->end(); ++it) {
        dump_chunk(stream, *it);
        chunks[*it] = i++;
    }

    for (vector<search_file*>::iterator it = files_.begin();
         it != files_.end(); ++it) {
        dump_file_contents(stream, chunks, *it);
    }

    stream_alignp(stream, kPageSize);
    hdr.chunks_off = stream.tellp();
    for (list<chunk*>::iterator it = alloc_->begin();
         it != alloc_->end(); ++it) {
        dump_chunk_data(stream, *it);
    }

    stream.seekp(0);
    stream.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
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
    stream.read(reinterpret_cast<char*>(&sf->oid), sizeof(sf->oid));
    sf->paths.resize(load_int32(stream));
    for (auto it = sf->paths.begin(); it != sf->paths.end(); ++it) {
        it->ref = refs_[load_int32(stream)];
        char *str = load_string(stream);
        it->path = str;
        delete[] str;
    }
    sf->no = files_.size();
    return sf;
}

void code_searcher::load_chunk(istream& stream, chunk *chunk) {
    chunk_header hdr;
    stream.read(reinterpret_cast<char*>(&hdr), sizeof hdr);
    assert(hdr.size <= kChunkSpace);
    chunk->size = hdr.size;

    /*
      uint32_t buf[3*hdr.nfiles];
      stream.read(reinterpret_cast<char*>(buf), sizeof buf);
    */
    for (int i = 0; i < hdr.nfiles; i++) {
        chunk->files.push_back(chunk_file());
        chunk_file &cf = chunk->files.back();
        uint32_t nfiles = load_int32(stream);
        for (int j = 0; j < nfiles; j++)
            cf.files.push_back(files_[load_int32(stream)]);
        cf.left  = load_int32(stream);
        cf.right = load_int32(stream);
    }
}

void code_searcher::load_chunk_data(int fd, istream& stream, chunk *chunk) {
    stream_aligng(stream, kPageSize);
    chunk::chunk_map.erase(chunk->data);
    delete[] chunk->data;
    chunk->data = static_cast<unsigned char*>
        (mmap(NULL, chunk->size, PROT_READ, MAP_SHARED,
              fd, stream.tellg()));
    assert(chunk->data != MAP_FAILED);
    chunk::chunk_map[chunk->data] = chunk;

    stream.seekg(chunk->size, ios_base::cur);
    stream_aligng(stream, kPageSize);

    chunk->suffixes = static_cast<uint32_t*>
        (mmap(NULL, chunk->size * sizeof(uint32_t), PROT_READ, MAP_SHARED,
              fd, stream.tellg()));
    stream.seekg(chunk->size * sizeof(uint32_t), ios_base::cur);
    chunk->build_tree();
}

void code_searcher::load_file_contents(std::istream& stream,
                                       vector<chunk*>& chunks,
                                       search_file *sf) {
    int npieces = load_int32(stream);
    uint32_t buf[3*npieces];

    stream.read(reinterpret_cast<char*>(buf), sizeof buf);
    sf->content.resize(npieces);

    for (int i = 0; i < npieces; i++) {
        chunk *chunk = chunks[buf[3*i]];
        char *p = reinterpret_cast<char*>(chunk->data) + buf[3*i + 1];
        int len = buf[3*i + 2];
        sf->content[i] = StringPiece(p, len);
    }
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

    stream.seekg(hdr.metadata_off);
    metadata_header meta;
    stream.read(reinterpret_cast<char*>(&meta), sizeof meta);

    for (int i = 0; i < meta.nrefs; i++) {
        refs_.push_back(load_string(stream));
    }

    for (int i = 0; i < meta.nfiles; i++) {
        files_.push_back(load_file(stream));
    }

    vector<chunk*> chunks;
    for (int i = 0; i < meta.nchunks; i++) {
        load_chunk(stream, alloc_->current_chunk());
        chunks.push_back(alloc_->current_chunk());
        if (i != meta.nchunks - 1)
            alloc_->skip_chunk();
    }

    streampos files_pos = stream.tellg();

    int fd = open(path.c_str(), O_RDONLY);
    assert(fd > 0);

    stream.seekg(hdr.chunks_off);
    for (list<chunk*>::iterator it = alloc_->begin();
         it != alloc_->end(); ++it) {
        load_chunk_data(fd, stream, *it);
    }

    close(fd);

    stream.seekg(files_pos);
    for (int i = 0; i < meta.nfiles; i++) {
        load_file_contents(stream, chunks, files_[i]);
    }


    finalized_ = true;
}
