#include <cstdlib>

#include "rdbms/utils/dynhash.hpp"

#include "rdbms/utils/alloc.hpp"
#include "rdbms/utils/math.hpp"

using namespace rdbms;

DynHashTable::DynHashTable(int nelements, HashCtl* hctl, int flags)
    : header_(nullptr),
      hash_(string_hash),
      seg_base_(nullptr),
      dir_(nullptr),
      context_(MemoryManager::context(MemCxtType::kDynHashContext)) {
  if (flags & HASH_FUNCTION) {
    hash_ = hctl->hash;
  }

  if (flags & HASH_SHARED_MEM) {
    // ctl structure is preallocated for shared memory tables. Note
    // that HASH_DIRSIZE had better be set as well.
    header_ = static_cast<HashHeader*>(hctl->header);
    seg_base_ = hctl->seg_base;
    context_ = hctl->context;
    dir_ = static_cast<SegOffset*>(hctl->dir);

    // Hash table already exists, we're just attaching to it.
    if (flags & HASH_ATTACH) {
      return;
    }
  }

  if (nullptr == header_) {
    header_ = static_cast<HashHeader*>(context_->alloc(sizeof(HashHeader)));
  }

  if (flags & HASH_SEGMENT) {
    header_->ssize = hctl->ssize;
    header_->sshift = ceil_log2(hctl->ssize);

    // ssize had better be a power of 2.
    assert(header_->ssize == (1L << header_->sshift));
  }

  if (flags & HASH_FFACTOR) {
    header_->ffactor = hctl->ffactor;
  }

  // SHM hash tables have fixed directory size passed by the caller.
  if (flags & HASH_DIRSIZE) {
    header_->max_dsize = hctl->max_dsize;
    header_->dsize = hctl->dsize;
  }

  // Hash table now allocates space for key and data but you have to say
  // how much space to allocate.
  if (flags & HASH_ELEM) {
    header_->key_size = hctl->key_size;
    header_->data_size = hctl->data_size;
  }

  if (flags & HASH_ALLOC) {
    context_ = hctl->context;
  }

  if (!init(nelements)) {
    destroy();
  }
}

void* DynHashTable::search(const char* key, HashAction action,
                           bool& out_found) {
  assert((action == kHashFind) || (action == kHashRemove) ||
         (action == kHashEnter) || (action == kHashFindSave) ||
         (action == kHashRemoveSaved));

  static struct State {
    Element* curr_elem;
    BucketIndex curr_index;
    BucketIndex* prev_index_ptr;
  } save_state;

  header_->accesses++;

  Element* curr;
  BucketIndex curr_index;
  BucketIndex* prev_index_ptr;

  if (action == kHashRemoveSaved) {
    curr = save_state.curr_elem;
    curr_index = save_state.curr_index;
    prev_index_ptr = save_state.prev_index_ptr;
  } else {
    auto bucket = compute_hash(key);
    auto segment_num = bucket >> header_->sshift;
    auto segment_ndx = MOD(bucket, header_->ssize);
    Segment segment = get_seg(segment_num);

    prev_index_ptr = &segment[segment_ndx];
    curr_index = *prev_index_ptr;

    // Follow collision chain.
    for (curr = nullptr; curr_index != INVALID_INDEX;) {
      // Coerce bucket index into a pointer.
      curr = get_bucket(curr_index);

      if (!std::memcmp(&(curr->opaque_data[0]), key, header_->key_size)) {
        break;
      }

      prev_index_ptr = &(curr->next);
      curr_index = *prev_index_ptr;
      header_->collisions++;
    }
  }

  // If we found an entry or if we weren't trying to insert, we're done
  // now.
  out_found = curr_index != INVALID_INDEX;

  switch (action) {
    case kHashEnter:
      if (curr_index != INVALID_INDEX) {
        return &(curr->opaque_data[0]);
      }
      break;

    case kHashRemove:
    case kHashRemoveSaved:
      if (curr_index != INVALID_INDEX) {
        assert(header_->nkeys > 0);
        header_->nkeys--;

        // Remove record from hash bucket's chain.
        *prev_index_ptr = curr->next;

        // Add the record to the freelist for this table.
        curr->next = header_->free_bucket_index;
        header_->free_bucket_index = curr_index;

        // Better hope the caller is synchronizing access to this
        // element, because someone else is going to reuse it the
        // next time something is added to the table.
        return &(curr->opaque_data[0]);
      }

      return nullptr;

    case kHashFind:
      if (curr_index != INVALID_INDEX) {
        return &(curr->opaque_data[0]);
      }

      return nullptr;

    case kHashFindSave:
      if (curr_index != INVALID_INDEX) {
        save_state.curr_elem = curr;
        save_state.prev_index_ptr = prev_index_ptr;
        save_state.curr_index = curr_index;

        return &(curr->opaque_data[0]);
      }

    default:
      return nullptr;
  }

  // If we got here, then we didn't find the element and we have to
  // insert it into the hash table.
  assert(curr_index == INVALID_INDEX);

  curr_index = header_->free_bucket_index;

  if (curr_index == INVALID_INDEX) {
    // No free elements. Allocate another chunk of buckets.
    if (!bucket_alloc()) {
      return nullptr;
    }

    curr_index = header_->free_bucket_index;
  }

  assert(curr_index != INVALID_INDEX);

  curr = get_bucket(curr_index);
  header_->free_bucket_index = curr->next;

  // Link into chain.
  *prev_index_ptr = curr_index;

  // Copy key into record.
  auto dest_addr = static_cast<char*>(&(curr->opaque_data[0]));
  std::memmove(dest_addr, key, header_->key_size);
  curr->next = INVALID_INDEX;

  // Check if it is time to split the segment.
  if (++header_->nkeys / (header_->max_bucket + 1) > header_->ffactor) {
    // NOTE: failure to expand table is not a fatal error, it just
    // means we have to run at higher fill factor than we wanted.
    expand_table();
  }

  return &(curr->opaque_data[0]);
}

void DynHashTable::destroy() {
  // Cannot destroy a shared memory hash table.
  assert(!seg_base_);
  statistic("destroy");

  int nsegs = header_->nsegs;

  for (auto seg_num = 0; nsegs > 0; nsegs--, seg_num++) {
    auto segment = get_seg(seg_num);
    auto seg_copy = segment;

    for (int j = 0; j < header_->ssize; j++) {
      for (auto p = *segment; p != INVALID_INDEX;) {
        auto bucket = get_bucket(p);
        auto next = bucket->next;
        context_->free(bucket);
        p = next;
      }
    }

    context_->free(seg_copy);
  }

  context_->free(dir_);
  context_->free(header_);
}

void DynHashTable::statistic(const char* where) const {
  fprintf(stderr, "%s: this HTAB -- accesses %ld collisions %ld\n", where,
          header_->accesses, header_->collisions);

  fprintf(stderr, "hash_stats: keys %ld keysize %ld maxp %d segmentcount %d\n",
          header_->nkeys, header_->key_size, header_->max_bucket,
          header_->nsegs);
  fprintf(stderr, "%s: total accesses %ld total collisions %ld\n", where,
          header_->accesses, header_->collisions);
  fprintf(stderr, "hash_stats: total expansions %ld\n", header_->expansions);
}

bool DynHashTable::init(int nelements) {
  // Divide number of elements by the fill factor to determine a desired
  // number of buckets. Allocate space for the next greater power of
  // two number of buckets
  nelements = (nelements - 1) / header_->ffactor + 1;
  auto nbuckets = 1 << ceil_log2(nelements);
  header_->max_bucket = nbuckets - 1;
  header_->low_mask = nbuckets - 1;
  header_->high_mask = (nbuckets << 1) - 1;

  // Figure number of directory segments needed, round up to a power of 2.
  auto nsegs = (nbuckets - 1) / header_->ssize + 1;
  nsegs = 1 << ceil_log2(nsegs);

  // Make sure directory is big enough. If pre-allocated directory is
  // too small, choke (caller screwed up).
  if (nsegs > header_->dsize) {
    if (!dir_) {
      header_->dsize = nsegs;
    } else {
      return false;
    }
  }

  // Allocate a directory.
  if (!dir_) {
    dir_ = static_cast<SegOffset*>(
        context_->alloc(header_->dsize * sizeof(SegOffset)));

    if (!dir_) {
      return false;
    }
  }

  // Allocate initial segments.
  for (auto segp = dir_; header_->nsegs < nsegs; header_->nsegs++, segp++) {
    *segp = seg_alloc();

    if (*segp == 0) {
      return false;
    }
  }

  fprintf(stderr,
          "%s\n%s%x\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%x\n%s%x\n%s%d\n%s%d\n",
          "init_htab:", "TABLE POINTER   ", this, "DIRECTORY SIZE  ",
          header_->dsize, "SEGMENT SIZE    ", header_->ssize,
          "SEGMENT SHIFT   ", header_->sshift, "FILL FACTOR     ",
          header_->ffactor, "MAX BUCKET      ", header_->max_bucket,
          "HIGH MASK       ", header_->high_mask, "LOW  MASK       ",
          header_->low_mask, "NSEGS           ", header_->nsegs,
          "NKEYS           ", header_->nkeys);

  return true;
}

SegOffset DynHashTable::seg_alloc() {
  Size size = sizeof(BucketIndex) * header_->ssize;
  auto segp = static_cast<Segment>(context_->alloc(size));

  if (!segp) {
    return 0;
  }

  std::memset(segp, 0, size);

  return make_hash_offset(segp);
}

bool DynHashTable::bucket_alloc() {
  Size bucket_sz = sizeof(BucketIndex) + header_->key_size + header_->data_size;
  bucket_sz = MAX_ALIGN(bucket_sz);

  auto tmp_bucket =
      static_cast<Element*>(context_->alloc(BUCKET_ALLOC_INCR * bucket_sz));

  if (!tmp_bucket) {
    return false;
  }

  // tmpIndex is the shmem offset into the first bucket of the array.
  BucketIndex tmp_index = make_hash_offset(tmp_bucket);
  BucketIndex last_index = header_->free_bucket_index;
  header_->free_bucket_index = tmp_index;

  // Initialize each bucket to point to the one behind it. NOTE: loop
  // sets last bucket incorrectly; we fix below.
  for (int i = 0; i < BUCKET_ALLOC_INCR; ++i) {
    tmp_bucket = get_bucket(tmp_index);
    tmp_index += bucket_sz;
    tmp_bucket->next = tmp_index;
  }

  // The last bucket points to the old freelist head (which is probably
  // invalid or we wouldn't be here).
  tmp_bucket->next = last_index;

  return true;
}

bool DynHashTable::expand_table() {
  int new_bucket = header_->max_bucket + 1;
  int new_segnum = new_bucket >> header_->sshift;
  int new_segndx = MOD(new_bucket, header_->ssize);

  if (new_segnum >= header_->nsegs) {
    // Allocate new segment if necessary -- could fail if dir full.
    if (new_segnum >= header_->dsize) {
      if (!dir_realloc()) {
        return false;
      }
    }

    if (!(dir_[new_segnum] = seg_alloc())) {
      return false;
    }

    header_->nsegs++;
  }

  // OK, we created a new bucket.
  header_->max_bucket++;

  // Before changing masks, find old bucket corresponding to same hash
  // values; values in that bucket may need to be relocated to new
  // bucket. Note that new_bucket is certainly larger than low_mask at
  // this point, so we can skip the first step of the regular hash mask
  // calc.
  int old_bucket = (new_bucket & header_->low_mask);

  // If we crossed a power of 2, readjust masks.
  if (new_bucket > header_->high_mask) {
    header_->low_mask = header_->high_mask;
    header_->high_mask = new_bucket | header_->low_mask;
  }

  // Relocate records to the new bucket. NOTE: because of the way the
  // hash masking is done in call_hash, only one old bucket can need to
  // be split at this point. With a different way of reducing the hash
  // value, that might not be true!
  int old_segnum = old_bucket >> header_->sshift;
  int old_segndx = MOD(old_bucket, header_->ssize);

  Segment old_seg = get_seg(old_segnum);
  Segment new_seg = get_seg(new_segnum);

  BucketIndex* old_bucket_idx = &(old_seg[old_segndx]);
  BucketIndex* new_bucket_idx = &(new_seg[new_segndx]);

  for (auto chain_index = *old_bucket_idx; chain_index != INVALID_INDEX;) {
    Element* chain = get_bucket(chain_index);
    BucketIndex next_index = chain->next;

    if (compute_hash(&(chain->opaque_data[0])) == old_bucket) {
      *old_bucket_idx = chain_index;
      old_bucket_idx = &(chain->next);
    } else {
      *new_bucket_idx = chain_index;
      new_bucket_idx = &(chain->next);
    }

    chain_index = next_index;
  }

  // Don't forget to terminate the rebuilt hash chains...
  *old_bucket_idx = INVALID_INDEX;
  *new_bucket_idx = INVALID_INDEX;

  return true;
}

bool DynHashTable::dir_realloc() {
  if (header_->max_dsize != NO_MAX_DSIZE) {
    return false;
  }

  int new_dsize = header_->dsize << 1;
  int old_dirsize = header_->dsize * sizeof(SegOffset);
  int new_dirsize = new_dsize * sizeof(SegOffset);

  auto new_dir = static_cast<SegOffset*>(context_->alloc(new_dirsize));

  if (new_dir != nullptr) {
    std::memmove(new_dir, dir_, old_dirsize);
    std::memset(new_dir + old_dirsize, 0, new_dirsize - old_dirsize);
    context_->free(dir_);
    dir_ = new_dir;
    header_->dsize = new_dsize;

    return true;
  }

  return false;
}