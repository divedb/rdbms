#include <algorithm>
#include <cassert>
#include <cstdlib>

#include "rdbms/utils/aset.hpp"

#include "rdbms/utils/elog.hpp"
#include <numeric>

namespace rdbms {

void AllocBlockData::memory_check() {
  Pointer start = static_cast<Pointer>(data());
  Pointer end = free_ptr();
  AllocSet set = static_cast<AllocSet>(aset());
  const char* name = set->name().c_str();

  while (start != end) {
    AllocChunk chunk = reinterpret_cast<AllocChunk>(start);
    Size chunk_size = chunk->size();
    Size data_size = chunk->requested_size();

    // Check chunk size.
    if (data_size > chunk_size) {
      elog(NOTICE,
           "%s: %s: requested size > allocated size for chunk %p in block %p",
           __func__, name, chunk, this);
    }

    if (chunk_size < (1 << AllocSetContext::kMinBits)) {
      elog(NOTICE, "%s: %s: bad size %lu for chunk %p in block %p", name,
           chunk_size, chunk, this);
    }

    // Single chunk block.
    if (chunk_size > AllocSetContext::kChunkLimit &&
        static_cast<void*>(chunk) != start) {
      elog(NOTICE, "%s: %s: bad single-chunk %p in block %p", __func__, name,
           chunk, this);
    }

    // If chunk is allocated, check for correct aset pointer. (If
    // it's free, the aset is the freelist pointer, which we can't
    // check as easily...)
    if (data_size > 0 && chunk->next() != static_cast<void*>(set)) {
      elog(NOTICE, "%s: %s: bogus aset link in block %p, chunk %p", __func__,
           name, this, chunk);
    }

    if (!chunk->memory_boundary_check()) {
      elog(NOTICE,
           "%s: %s: detected write past chunk end in block %p, chunk %p",
           __func__, name, this, chunk);
    }

    start += chunk_size + kChunkHdrSz;
  }
}

AllocSetContext::AllocSetContext(MemoryContext parent, std::string name,
                                 Size min_context_size, Size init_block_size,
                                 Size max_block_size)
    : MemoryContextData(kAllocSetContext, parent, std::move(name)),
      keeper_{},
      freelist_{} {
  init_block_size = MAX_ALIGN(init_block_size);
  max_block_size = MAX_ALIGN(max_block_size);

  init_block_size_ = std::max({init_block_size, kMinBlockSize});
  max_block_size_ = std::max({max_block_size, init_block_size_});

  if (min_context_size > kBlockHdrSz + kChunkHdrSz) {
    Size blk_size = MAX_ALIGN(min_context_size);
    Memory mem = MemoryPool::allocate(blk_size);
    keeper_ = ::new (AllocBlockData)(this, mem);
  }
}

void* AllocSetContext::alloc(Size size) {
  // If requested size exceeds maximum for chunks, allocate an entire
  // block for this request.
  if (size > kChunkLimit) {
    auto chunk = this->alloc_large_chunk(size);

    assert(chunk != nullptr);

    return chunk->data();
  }

  if (auto chunk = try_alloc_from_freelist(size); chunk != nullptr) {
    return chunk->data();
  }

  auto chunk = alloc_from_block(size);

  assert(chunk != nullptr);

  return chunk->data();
}

void AllocSetContext::free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }

  AllocChunk chunk = AllocChunkData::cast_ptr(ptr);
  assert(static_cast<void*>(chunk->next()) == this);

  if (chunk->size() > kChunkLimit) {
    this->free_large_chunk(chunk);
  } else {
    return_chunk_to_freelist(chunk);
  }
}

void* AllocSetContext::realloc(void* ptr, Size size) {
  AllocChunk chunk = AllocChunkData::cast_ptr(ptr);

  if (!chunk->memory_boundary_check()) {
    // TODO(gc): fix later
    fprintf(stderr, "%s: detected write past chunk end in %s %p", __func__,
            name_, chunk);
    exit(1);
  }

  Size old_size = chunk->size();

  // Chunk sizes are aligned to power of 2 in alloc(). Maybe the
  // allocated area already is >= the new size. (In particular, we
  // always fall out here if the requested size is a decrease.)
  if (old_size >= size) {
    chunk->set_requested_size(size);

    return ptr;
  }

  if (old_size > kChunkLimit) {
    void* block = static_cast<Pointer>(static_cast<void*>(chunk)) - kBlockHdrSz;

    if (!blocks_.remove(block)) {
      fprintf(stderr, "%s: cannot find block containing chunk %p\n", __func__,
              chunk);
      exit(1);
    }

    Size chunk_size = MAX_ALIGN(size);
    Size blk_size = kBlockHdrSz + kChunkHdrSz + chunk_size;
    Memory mem = MemoryPool::reallocate(block, blk_size);
    auto new_block = ::new (AllocBlockData)(this, mem);
    chunk = new_block->fetch_chunk(chunk_size, size);
    blocks_.enqueue(new_block);

    return chunk->data();
  } else {
    return_chunk_to_freelist(chunk);

    return alloc(size);
  }
}

void AllocSetContext::reset() {
  std::memset(freelist_, 0, sizeof freelist_);

  for (auto block : blocks_) {
    MemoryPool::deallocate(block);
  }

  blocks_.reset();

  if (keeper_ != nullptr) {
    keeper_->reset();
  }
}

void AllocSetContext::destroy() {
  reset();
  MemoryPool::deallocate(keeper_);
  keeper_ = nullptr;
}

void AllocSetContext::check() {
  for (auto block : blocks_) {
    block->memory_check();
  }
}

void AllocSetContext::stats() {
  Size nblocks = blocks_.size();
  Size total_space = std::accumulate(
      blocks_.begin(), blocks_.end(), 0,
      [](Size init, AllocBlock block) { return init + block->size(); });
  Size free_space = std::accumulate(
      blocks_.begin(), blocks_.end(), 0,
      [](Size init, AllocBlock block) { return init + block->avail_space(); });

  Size nchunks = 0;

  for (auto chunk : freelist_) {
    while (chunk != nullptr) {
      nchunks++;
      free_space += chunk->size() + kChunkHdrSz;
      chunk = chunk->next();
    }
  }

  if (keeper_ != nullptr) {
    nblocks++;
    total_space += keeper_->size();
    free_space += keeper_->avail_space();
  }

  fprintf(stderr,
          "%s: %ld total in %ld blocks; %ld free (%ld chunks); %ld used\n",
          name_.c_str(), total_space, nblocks, free_space, nchunks,
          total_space - free_space);
}

AllocChunk AllocSetContext::alloc_large_chunk(Size size) {
  Size chunk_size = MAX_ALIGN(size);
  Size blk_size = kBlockHdrSz + kChunkHdrSz + chunk_size;

  Memory mem = MemoryPool::allocate(blk_size);
  auto block = ::new (AllocBlockData)(this, mem);
  blocks_.enqueue(block);

  return block->fetch_chunk(chunk_size, size);
}

AllocChunk AllocSetContext::try_alloc_from_freelist(Size size) {
  AllocChunk chunk;
  AllocChunk prior_free = nullptr;

  int fidx = free_index(size);

  // Request is small enough to be treated as a chunk.  Look in the
  // corresponding free list to see if there is a free chunk we could
  // reuse.
  for (chunk = freelist_[fidx]; chunk != nullptr; chunk = chunk->next()) {
    if (chunk->size() >= size) {
      break;
    }

    prior_free = chunk;
  }

  // If not found.
  if (chunk == nullptr) {
    return chunk;
  }

  // If one is found, remove it from the free list, make it again a
  // member of the alloc set and return its data address.
  AllocChunk next = chunk->next();

  if (prior_free == nullptr) {
    freelist_[fidx] = next;
  } else {
    prior_free->set_next(next);
  }

  chunk->set_next(this);

  // In PostgreSQL version 7.1.3, this functionality is enabled by the
  // MEMORY_CONTEXT_CHECKING macro. We opt for enabling it despite the
  // efficiency loss, as it significantly enhances debuggability.
  chunk->set_requested_size(size);

  return chunk;
}

AllocChunk AllocSetContext::alloc_from_block(Size size) {
  AllocBlock block = blocks_;
  int fidx = free_index(size);
  Size chunk_size = 1 << (fidx + kMinBits);

  if (block != nullptr && block->avail_space() < (chunk_size + kChunkHdrSz)) {
    merge_block_remainder_to_chunk(block);
    block = nullptr;
  }

  if (block == nullptr) {
    Size blk_size;

    // TODO(gc): does this really makes sense?
    if (blocks_ == nullptr) {
      blk_size = init_block_size_;
    } else {
      blk_size = blocks_->size();

      // Special case: if very first allocation was for a large
      // chunk (or we have a small "keeper" block), could have an
      // undersized top block. Do something reasonable.
      if (blk_size < init_block_size_) {
        blk_size = init_block_size_;
      } else {
        blk_size <<= 1;

        if (blk_size > max_block_size_) {
          blk_size = max_block_size_;
        }
      }
    }

    Size required_size = kBlockHdrSz + kChunkHdrSz + chunk_size;

    if (blk_size < required_size) {
      blk_size = required_size;
    }

    Memory mem = MemoryPool::allocate(blk_size);
    block = ::new (AllocBlockData)(this, mem);
    block->set_next(blocks_);
    blocks_ = block;
  }

  return block->fetch_chunk(chunk_size, size);
}

void AllocSetContext::merge_block_remainder_to_chunk(AllocBlock block) {
  assert(block != nullptr);

  auto avail_space = block->avail_space();
  // The existing active (top) block does not have enough room
  // for the requested allocation, but it might still have a
  // useful amount of space in it. Once we push it down in the
  // block list, we'll never try to allocate more space from it.
  // So, before we do that, carve up its free space into chunks
  // that we can put on the set's freelists.
  //
  // Because we can only get here when there's less than
  // ALLOC_CHUNK_LIMIT left in the block, this loop cannot
  // iterate more than ALLOCSET_NUM_FREELISTS-1 times.
  while (avail_space >= ((1 << kMinBits) + kChunkHdrSz)) {
    Size chunk_size = avail_space - kChunkHdrSz;
    int fidx = free_index(chunk_size);

    // In most cases, we'll get back the index of the next
    // larger freelist than the one we need to put this chunk
    // on. The exception is when availchunk is exactly a
    // power of 2.
    if (chunk_size != (1 << (fidx + kMinBits))) {
      fidx--;
      chunk_size = (1 << (fidx + kMinBits));
    }

    auto chunk = block->fetch_chunk(chunk_size, 0);
    return_chunk_to_freelist(chunk);
    avail_space = block->avail_space();
  }
}

void AllocSetContext::return_chunk_to_freelist(AllocChunk chunk) {
  int fidx = free_index(chunk->size());
  chunk->clear();
  chunk->set_next(freelist_[fidx]);
  freelist_[fidx] = chunk;
}

void AllocSetContext::free_large_chunk(AllocChunk chunk) {
  // Big chunks are certain to have been allocated as single-chunk
  // blocks. Find the containing block and return it to malloc().
  void* block = static_cast<Pointer>(static_cast<void*>(chunk)) - kBlockHdrSz;

  if (!blocks_.remove(block)) {
    // TODO(gc): fix later
    fprintf(stderr, "%s: cannot find block containing chunk %p\n", __func__,
            chunk);
    exit(1);
  }

  MemoryPool::deallocate(block);
}

}  // namespace rdbms