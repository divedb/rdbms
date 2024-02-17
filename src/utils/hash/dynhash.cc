#include <cstdlib>

#include "rdbms/utils/alloc.hpp"
#include "rdbms/utils/hsearch.hpp"
#include "rdbms/utils/math.hpp"

namespace rdbms {

#define MOD(x, y) ((x) & ((y)-1))

inline constexpr int kInvalidIndex = 0;
inline constexpr int kNoMaxDSize = -1;

struct DynHashTable {
  HashHeader* header;
  HashFunc hash;
  Pointer seg_base;
  Directory dir;
  MemoryContext context;
};

static void hash_default(DynHashTable* htab);
static bool hash_init(DynHashTable* htab, int nelem);
static Size compute_hash(DynHashTable* htab, const char* key);
static Segment* make_segment(DynHashTable* htab);
static bool expand_bucket(DynHashTable* htab);
static bool expand_table(DynHashTable* htab);
static bool dir_realloc(DynHashTable* htab);

DynHashTable* hash_create(int nelem, HashCtl* hctl, HashFlag flags) {
  auto mem = MemoryPool::allocate(sizeof(DynHashTable));
  auto htab = static_cast<DynHashTable*>(mem.ptr);

  if (flags & kHashFunction) {
    htab->hash = hctl->hash;
  } else {
    htab->hash = string_hash;
  }

  if (flags & kHashSharedMem) {
    // ctl structure is preallocated for shared memory tables. Note
    // that HASH_DIRSIZE had better be set as well.
    htab->header = static_cast<HashHeader*>(hctl->header);
    htab->seg_base = hctl->seg_base;
    htab->context = hctl->context;
    htab->dir = hctl->dir;

    // Hash table already exists, we're just attaching to it.
    if (flags & kHashAttach) {
      return htab;
    }
  } else {
    // Set up hash table defaults.
    htab->header = nullptr;
    htab->context = MemoryManager::context(MemCxtType::kDynHashContext);
    htab->dir = nullptr;
    htab->seg_base = nullptr;
  }

  if (htab->header == nullptr) {
    htab->header =
        static_cast<HashHeader*>(htab->context->alloc(sizeof(*(htab->header))));
  }

  hash_default(htab);

  auto header = htab->header;

  if (flags & kHashSegment) {
    header->ssize = hctl->ssize;
    header->sshift = ceil_log2(header->ssize);

    assert(header->ssize == (1L << header->sshift));
  }

  if (flags & kHashFillFactor) {
    header->ffactor = hctl->ffactor;
  }

  // SHM hash tables have fixed directory size passed by the caller.
  if (flags & kHashDirSize) {
    header->dsize = hctl->dsize;
    header->max_dsize = hctl->max_dsize;
  }

  // Hash table now allocates space for key and data but you have to say
  // how much space to allocate.
  if (flags & kHashElem) {
    header->key_size = hctl->key_size;
    header->data_size = hctl->data_size;
  }

  if (flags & kHashAlloc) {
    htab->context = hctl->context;
  }

  if (hash_init(htab, nelem)) {
    return nullptr;
  }

  return htab;
}

void hash_destroy(DynHashTable* htab) {
  if (htab == nullptr) {
    return;
  }

  // Cannot destroy a shared memory hash table.
  assert(!htab->seg_base);

  auto context = MemoryManager::context(MemCxtType::kDynHashContext);
  htab_stats("destroy", htab);
  auto header = htab->header;

  for (int i = 0; i < header->nsegs; i++) {
    auto segment = htab->dir[i];

    for (int j = 0; j < header->ssize; j++) {
      auto element = segment[j];

      while (element != nullptr) {
        auto next = element->next;
        context->free(element);
        element = next;
      }
    }
  }

  context->free(htab->dir);
  context->free(htab->header);
  context->free(htab);
}

void hash_statistic(const char* where, DynHashTable* htab) {
  fprintf(stderr, "%s: this HTAB -- accesses %ld collisions %ld\n", where,
          htab->header->accesses, htab->header->collisions);

  fprintf(stderr, "hash_stats: keys %ld keysize %ld maxp %d segmentcount %d\n",
          htab->header->nkeys, htab->header->key_size, htab->header->max_bucket,
          htab->header->nsegs);
  fprintf(stderr, "%s: total accesses %ld total collisions %ld\n", where,
          htab->header->accesses, htab->header->collisions);
  fprintf(stderr, "hash_stats: total expansions %ld\n",
          htab->header->expansions);
}

void* hash_search(DynHashTable* htab, const char* key, HashAction action,
                  bool& out_found) {
  static struct State {
    Element* curr{};
    Element** prev_ptr{};
  } save_state;

  Size bucket;
  Element* curr;
  Element** prev_ptr;
  auto header = htab->header;

  header->accesses++;

  if (action == kHashRemoveSaved) {
    curr = save_state.curr;
    prev_ptr = save_state.prev_ptr;
  } else {
    Size segment_num = bucket >> header->sshift;
    Size segment_ndx = MOD(bucket, header->ssize);
    Segment* segment = htab->dir[segment_num];

    // Follow collision chain.
    curr = segment[segment_ndx];
    prev_ptr = &segment[segment_ndx];

    while (curr != nullptr) {
      if (!std::memcmp(&(curr->opaque_data[0]), key, header->key_size)) {
        break;
      }

      prev_ptr = &(curr->next);
      curr = *prev_ptr;
      header->collisions++;
    }
  }

  out_found = curr != nullptr;

  switch (action) {
    case kHashEnter:
      if (curr != nullptr) {
        return &(curr->opaque_data[0]);
      }

      break;
    case kHashRemove:
    case kHashRemoveSaved:
      if (curr != nullptr) {
        assert(header->nkeys > 0);

        header->nkeys--;

        // Remove record from hash bucket's chain.
        *prev_ptr = curr->next;
        // Add the record to the freelist for this table.
        curr->next = header->free_bucket;
        header->free_bucket = curr;

        // Better hope the caller is synchronizing access to this
        // element, because someone else is going to reuse it the
        // next time something is added to the table.
        return &(curr->opaque_data[0]);
      }

      return nullptr;

    case kHashFind:
      if (curr != nullptr) {
        return &(curr->opaque_data[0]);
      }

      return nullptr;

    case kHashFindSave:
      if (curr != nullptr) {
        save_state.curr = curr;
        save_state.prev_ptr = prev_ptr;

        return &(curr->opaque_data[0]);
      }

      return nullptr;

    default:
      return nullptr;
  }

  // If we got here, then we didn't find the element and we have to
  // insert it into the hash table.
  assert(curr == nullptr);

  // Get the next free bucket.
  curr = header->free_bucket;

  if (curr == nullptr) {
    if (!expand_bucket(htab)) {
      return nullptr;
    }

    curr = header->free_bucket;
  }

  assert(curr != nullptr);

  curr->next = nullptr;
  header->free_bucket = curr->next;
  // Link into chain.
  *prev_ptr = curr;
  auto dest = &(curr->opaque_data[0]);
  std::memmove(dest, key, header->key_size);

  // Check if it is time to split the segment.
  if (++header->nkeys / (header->max_bucket + 1) > header->ffactor) {
    // NOTE: failure to expand table is not a fatal error, it just
    // means we have to run at higher fill factor than we wanted.
    expand_table(htab);
  }

  return &(curr->opaque_data[0]);
}

static void hash_default(DynHashTable* htab) {
  auto header = htab->header;

  header->dsize = HashHeader::kDefDirSize;
  header->ssize = HashHeader::kDefSegSize;
  header->sshift = HashHeader::kDefSegSizeShift;
  header->max_bucket = 0;
  header->high_mask = (HashHeader::kDefDirSize << 1) - 1;
  header->low_mask = (HashHeader::kDefDirSize - 1);

  header->ffactor = HashHeader::kDefFillFactor;
  header->nkeys = 0;
  header->nsegs = 0;

  header->key_size = sizeof(Pointer);
  header->data_size = sizeof(Pointer);
  header->max_dsize = kNoMaxDSize;
  header->free_bucket = nullptr;
  header->accesses = 0;
  header->collisions = 0;
}

static bool hash_init(DynHashTable* htab, int nelem) {
  HashHeader* header = htab->header;

  // Divide number of elements by the fill factor to determine a desired
  // number of buckets. Allocate space for the next greater power of
  // two number of buckets
  nelem = (nelem - 1) / header->ffactor + 1;
  int nbuckets = 1 << ceil_log2(nelem);
  header->max_bucket = nbuckets - 1;
  header->low_mask = nbuckets - 1;
  header->high_mask = (nbuckets << 1) - 1;

  // Figure number of directory segments needed, round up to a power of 2.
  int nsegs = (nbuckets - 1) / header->ssize + 1;
  nsegs = 1 << ceil_log2(nsegs);

  // Make sure directory is big enough. If pre-allocated directory is
  // too small, choke (caller screwed up).
  if (nsegs > header->dsize) {
    if (htab->dir == nullptr) {
      header->dsize = nsegs;
    } else {
      return false;
    }
  }

  // Allocate a directory.
  if (htab->dir == nullptr) {
    Size size = header->dsize * sizeof(Segment*);
    htab->dir = static_cast<Directory>(htab->context->alloc(size));
  }

  // Allocate initial segments.
  for (auto segp = htab->dir; header->nsegs < nsegs; header->nsegs++, segp++) {
    *segp = make_segment(htab);
  }

  // TODO(gc): fix later.
  fprintf(stderr,
          "%s\n%s%x\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%x\n%s%x\n%s%d\n%s%d\n",
          "init_htab:", "TABLE POINTER   ", htab, "DIRECTORY SIZE  ",
          header->dsize, "SEGMENT SIZE    ", header->ssize, "SEGMENT SHIFT   ",
          header->sshift, "FILL FACTOR     ", header->ffactor,
          "MAX BUCKET      ", header->max_bucket, "HIGH MASK       ",
          header->high_mask, "LOW  MASK       ", header->low_mask,
          "NSEGS           ", header->nsegs, "NKEYS           ", header->nkeys);

  return true;
}

static Size compute_hash(DynHashTable* htab, const char* key) {
  auto header = htab->header;
  Size hash = htab->hash(key, header->key_size);
  Size bucket = hash & header->high_mask;

  if (bucket > header->max_bucket) {
    bucket = bucket & header->low_mask;
  }

  return bucket;
}

static Segment* make_segment(DynHashTable* htab) {
  auto context = htab->context;
  Size size = sizeof(Element*) * htab->header->ssize;
  Segment* segment = static_cast<Segment*>(context->alloc(size));
  std::memset(segment, 0, size);

  return segment;
}

static bool expand_bucket(DynHashTable* htab) {
  auto header = htab->header;
  auto context = htab->context;
  Size bucket_size = sizeof(Element*) + header->key_size + header->data_size;
  bucket_size = MAX_ALIGN(bucket_size);

  auto element = static_cast<Element*>(
      context->alloc(HashHeader::kBucketAllocIncr * bucket_size));

  if (element == nullptr) {
    return false;
  }

  // Initialize each bucket to point to the one behind it. NOTE: loop
  // sets last bucket incorrectly; we fix below.
  for (int i = 0; i < HashHeader::kBucketAllocIncr - 1; ++i) {
    element[i].next = &element[i + 1];
  }

  element[HashHeader::kBucketAllocIncr - 1].next = nullptr;
  header->free_bucket = element;

  return true;
}

static bool expand_table(DynHashTable* htab) {
  auto header = htab->header;
  Size new_bucket = header->max_bucket + 1;
  Size new_segnum = new_bucket >> header->sshift;
  Size new_segndx = MOD(new_bucket, header->ssize);

  if (new_segnum >= header->nsegs) {
    // Allocate new segment if necessary -- could fail if dir full.
    if (new_segnum >= header->dsize) {
      if (!dir_realloc(htab)) {
        return false;
      }
    }

    if (!(htab->dir[new_segnum] = make_segment(htab))) {
      return false;
    }

    header->nsegs++;
  }

  // OK, we created a new bucket.
  header->max_bucket++;

  // *Before* changing masks, find old bucket corresponding to same hash
  // values; values in that bucket may need to be relocated to new
  // bucket. Note that new_bucket is certainly larger than low_mask at
  // this point, so we can skip the first step of the regular hash mask
  // calc.
}

static bool dir_realloc(DynHashTable* htab) {
  auto header = htab->header;

  if (header->max_dsize != HashHeader::kNoMaxDsize) {
    return false;
  }

  auto context = htab->context;
  Size new_dsize = header->dsize << 1;
  Size old_dirsize = header->dsize * sizeof(Segment*);
  Size new_dirsize = new_dsize * sizeof(Segment*);

  auto odir = reinterpret_cast<Pointer>(htab->dir);
  auto ndir = static_cast<Pointer>(context->alloc(new_dirsize));

  if (ndir != nullptr) {
    std::memmove(ndir, odir, old_dirsize);
    std::memset(ndir + old_dirsize, 0, new_dirsize - old_dirsize);
    context->free(odir);
    htab->dir = reinterpret_cast<Directory>(ndir);
    header->dsize = new_dsize;

    return true;
  }

  return false;
}

}  // namespace rdbms