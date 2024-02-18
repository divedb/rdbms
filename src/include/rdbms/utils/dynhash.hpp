#pragma once

#include "rdbms/postgres.hpp"
#include "rdbms/utils/mmgr.hpp"

namespace rdbms {

// Fast MOD arithmetic, assuming that y is a power of 2.
#define MOD(x, y) ((x) & ((y)-1))

// A hash table has a top-level "directory", each of whose entries points
// to a "segment" of ssize bucket headers. The maximum number of hash
// buckets is thus dsize * ssize (but dsize may be expansible). Of course,
// the number of records in the table can be larger, but we don't want a
// whole lot of records per bucket or performance goes down.
//
// In a hash table allocated in shared memory, the directory cannot be
// expanded because it must stay at a fixed address. The directory size
// should be selected using hash_select_dirsize (and you'd better have
// a good idea of the maximum number of entries!). For non-shared hash
// tables, the initial directory size can be left at the default.
#define DEF_SEGSIZE       256
#define DEF_SEGSIZE_SHIFT 8  // Must be log2(DEF_SEGSIZE)
#define DEF_DIRSIZE       256
#define DEF_FFACTOR       1  // Default fill factor

#define PRIME1 37  // For the hash function
#define PRIME2 1048583

// Hash bucket is actually bigger than this. Key field can have
// variable length and a variable length data field follows it.
struct Element {
  BucketIndex next;
  char opaque_data[];
};

using HashFunc = Size (*)(const char*, int);
using BucketIndex = Size;
using Segment = BucketIndex*;
using SegOffset = Size;

struct HashHeader {
  int dsize{DEF_DIRSIZE};          // Directory size
  int ssize{DEF_SEGSIZE};          // Segment size -- must be power of 2
  int sshift{DEF_SEGSIZE_SHIFT};   // Segment shift
  int max_bucket{};                // ID of maximum bucket in use
  int high_mask{};                 // Mask to module into entire table
  int low_mask{};                  // Mask to module into lower half of table
  int ffactor{DEF_FFACTOR};        // Fill factor
  int nkeys{};                     // Number of keys in hash table
  int nsegs{};                     // Number of allocated segments
  int key_size{sizeof(Pointer)};   // Hash key length in bytes
  int data_size{sizeof(Pointer)};  // Element data length in bytes
  int max_dsize{NO_MAX_DSIZE};     // 'dsize' limit if directory is fixed size
  BucketIndex free_bucket_index{INVALID_INDEX};  // Index of first free bucket

  Size accesses{};
  Size collisions{};
  Size expansions{};
};

class DynHashTable {
 public:
  DynHashTable(int nelements, HashCtl* hctl, int flags);
  void* search(const char* key, HashAction action, bool& out_found);

  void destroy();
  void statistic(const char* where) const;

 private:
  bool init(int nelements);
  SegOffset seg_alloc();
  bool bucket_alloc();

  SegOffset make_hash_offset(void* ptr) const {
    return static_cast<Pointer>(ptr) - seg_base_;
  }

  Segment get_seg(int seg_num) const {
    return reinterpret_cast<Segment>(seg_base_ + dir_[seg_num]);
  }

  Element* get_bucket(BucketIndex bucket_offs) const {
    return reinterpret_cast<Element*>(seg_base_ + bucket_offs);
  }

  u32 compute_hash(const char* key) {
    u32 hashv = hash_(key, header_->key_size);
    auto bucket = hashv & header_->high_mask;

    if (bucket > header_->max_bucket) {
      bucket = bucket & header_->low_mask;
    }

    return bucket;
  }

  bool expand_table();
  bool dir_realloc();

  HashHeader* header_;     // Shared control information
  HashFunc hash_;          // Hash function
  Pointer seg_base_;       // Segment base address for calculating point values
  SegOffset* dir_;         // Directory of segments start
  MemoryContext context_;  // Memory allocator
};

struct HashCtl {
  int ssize;              // Segment size
  int dsize;              // Directory size
  int ffactor;            // Fill factor
  int key_size;           // Hash key length in bytes
  int data_size;          // Element data length in bytes
  int max_dsize;          // Limit to dsize if directory size is limited
  HashFunc hash;          // Hash function
  Pointer seg_base;       // Base for calculating bucket + seg ptrs
  MemoryContext context;  // Memory allocation function
  void* dir;              // Directory if allocated already
  void* header;           // Location of header information in shared memory
};

#define HASH_SEGMENT    0x002  // Setting segment size
#define HASH_DIRSIZE    0x004  // Setting directory size
#define HASH_FFACTOR    0x008  // Setting fill factor
#define HASH_FUNCTION   0x010  // Set user defined hash function
#define HASH_ELEM       0x020  // Setting key/data size
#define HASH_SHARED_MEM 0x040  // Setting shared mem const
#define HASH_ATTACH     0x080  // Do not initialize hctl
#define HASH_ALLOC      0x100  // Setting memory allocator

// seg_alloc assumes that INVALID_INDEX is 0.
#define INVALID_INDEX (0)
#define NO_MAX_DSIZE  (-1)

// Number of hash buckets allocated at once.
#define BUCKET_ALLOC_INCR (30)

enum HashAction {
  kHashFind,
  kHashEnter,
  kHashRemove,
  kHashFindSave,
  kHashRemoveSaved
};

Size string_hash(const char* key, int size);
Size tag_hash(const char* key, int size);

}  // namespace rdbms