#pragma once

#include "rdbms/postgres.hpp"
#include "rdbms/utils/mmgr.hpp"

namespace rdbms {

using HashFunc = Size (*)(const char*, int);

struct Element {
  Element* next;
  char opaque_data[];
};

using Segment = Element*;
using Directory = Segment**;

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
struct HashHeader {
  static constexpr int kNoMaxDsize = -1;
  static constexpr int kDefSegSize = 256;
  static constexpr int kDefSegSizeShift = 8;
  static constexpr int kDefDirSize = 256;
  static constexpr int kDefFillFactor = 1;
  static constexpr int kBucketAllocIncr = 30;

  int dsize;             // Directory size
  int ssize;             // Segment size -- must be power of 2
  int sshift;            // Segment shift
  int max_bucket;        // ID of maximum bucket in use
  int high_mask;         // Mask to module into entire table
  int low_mask;          // Mask to module into lower half of table
  int ffactor;           // Fill factor
  int nkeys;             // Number of keys in hash table
  int nsegs;             // Number of allocated segments
  int key_size;          // Hash key length in bytes
  int data_size;         // Element data length in bytes
  int max_dsize;         // 'dsize' limit if directory is fixed size
  Element* free_bucket;  // Index of first free bucket

  Size accesses;
  Size collisions;
  Size expansions;
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
  Directory dir;          // Directory if allocated already
  void* header;           // Location of header information in shared memory
};

enum HashFlag {
  kHashSegment = 0x002,     // Setting segment size
  kHashDirSize = 0x004,     // Setting directory size
  kHashFillFactor = 0x008,  // Setting fill factor
  kHashFunction = 0x010,    // Setting user defined hash function
  kHashElem = 0x020,        // Setting key/data size
  kHashSharedMem = 0x040,   // Setting shared memory
  kHashAttach = 0x080,      // Don't initialize hctl
  kHashAlloc = 0x100        // Setting memory allocator
};

enum HashAction {
  kHashFind,
  kHashEnter,
  kHashRemove,
  kHashFindSave,
  kHashRemoveSaved
};

struct DynHashTable;

DynHashTable* hash_create(int nelem, HashCtl* hctl, HashFlag flags);
void hash_destroy(DynHashTable* htab);
void hash_statistic(const char* where, DynHashTable* htab);
void* hash_search(DynHashTable* htab, const char* key, HashAction action,
                  bool& out_found);

Size string_hash(const char* key, int size);
Size tag_hash(const char* key, int size);

}  // namespace rdbms