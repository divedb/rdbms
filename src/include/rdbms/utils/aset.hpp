#pragma once

#include "rdbms/utils/alloc.hpp"
#include "rdbms/utils/mcxt.hpp"

namespace rdbms {

using AllocSet = struct AllocSetContext*;
using AllocBlock = struct AllocBlockData*;
using AllocChunk = struct AllocChunkData*;
using ConstAllocBlock = const struct AllocBlockData*;

template <typename T>
struct HeaderBase {
 public:
  using value_type = T;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_referene = const value_type&;

  static constexpr Size kMinSize = MAX_ALIGN(kHdrSize);

  reference header() {
    void* base = static_cast<Pointer>(data_) - kMinSize;
    return *static_cast<pointer>(base);
  }

  const_referene header() const {
    const void* base = static_cast<Pointer>(data_) - kMinSize;

    return *static_cast<const_pointer>(base);
  }

  const void* data() const { return data_; }
  void* data() { return data_; }

 protected:
  HeaderBase(const T& header, Memory mem) {
    assert(mem.size >= kMinSize);

    std::memcpy(mem.ptr, std::addressof(header), kHdrSize);
    data_ = static_cast<Pointer>(mem.ptr) + kMinSize;
  }

  ConstPointer base() const { return static_cast<Pointer>(data_) - kMinSize; }

 private:
  static constexpr Size kHdrSize = sizeof(T);

  void* data_;
};

static inline constexpr int kChunkHdrSz =
    HeaderBase<AllocChunkHeader>::kMinSize;

static inline constexpr int kBlockHdrSz =
    HeaderBase<AllocBlockHeader>::kMinSize;

// Each chunk consists of header and data. The header keeps track of allocated
// memory size and requested size. The actual allocated size may be greater than
// the requested size when taking the alignment into consideration. The `next`
// field could be next chunk when this chunk is inisde the free list or block
// when it's used for the client. Note that: chunk doesn't own the data. The
// caller should be responsible for cleaning the data.
struct AllocChunkHeader {
  void* next;
  Size size;
  Size requested_size;
};

// Chunk freelist k holds chunks of size 1 << (k + ALLOC_MINBITS),
// for k = 0 .. ALLOCSET_NUM_FREELISTS-1.
//
// Note that all chunks in the freelists have power-of-2 sizes.  This
// improves recyclability: we may waste some space, but the wasted space
// should stay pretty constant as requests are made and released.
//
// A request too large for the last freelist is handled by allocating a
// dedicated block from malloc().  The block still has a block header and
// chunk header, but when the chunk is freed we'll return the whole block
// to malloc(), not put it on our freelists.
//
// CAUTION: ALLOC_MINBITS must be large enough so that
// 1<<ALLOC_MINBITS is at least MAXALIGN,
// or we may fail to align the smallest chunks adequately.
// 16-byte alignment is enough on all currently known machines.
//
// With the current parameters, request sizes up to 8K are treated as chunks,
// larger requests go into dedicated blocks.  Change ALLOCSET_NUM_FREELISTS
// to adjust the boundary point.

struct AllocChunkData : public HeaderBase<AllocChunkHeader> {
 public:
  using Base = HeaderBase<AllocChunkHeader>;

  static constexpr int kMagic = 0x7E;
  static constexpr int kDirty = 0x7F;

  // This function is invoked when the client frees the chunk.
  // It serves to cast the pointer back to its original chunk type.
  static AllocChunk cast_ptr(void* ptr) {
    return reinterpret_cast<AllocChunk>(static_cast<Pointer>(ptr) -
                                        kChunkHdrSz);
  }

  // Constructor.
  AllocChunkData(Memory mem, void* next, Size requested_size)
      : Base({next, mem.size, requested_size}, mem) {}

  AllocChunk next() { return static_cast<AllocChunk>(this->header().next); }
  Size size() const { return this->header().size; }
  Size requested_size() const { return header().requested_size; }

  void set_next(void* next) { this->header().next = next; }
  void set_requested_size(Size requested_size) {
    this->header().requested_size = requested_size;
    this->mark_memory_boundary(requested_size);
  }

  // Marking this chunk as cleared signifies that it has been freed and is
  // likely to be reintroduced into the freelist.
  void clear() {
    set_requested_size(0);
    clobber_memory();
  }

  bool memory_boundary_check() const {
    if (auto& hdr = this->header(); hdr.requested_size < hdr.size) {
      return static_cast<ConstPointer>(data())[hdr.requested_size] == kMagic;
    }

    return true;
  }

 private:
  void mark_memory_boundary(Size requested_size) {
    if (requested_size < this->size()) {
      static_cast<Pointer>(data())[requested_size] = kMagic;
    }
  }

  // This function will help to debugg.
  // This scenario occurs when the client releases a chunk and the chunk might
  // be returned to the freelist. If we overwrite the memory and the client
  // attempts to reuse the previously freed memory, the behavior becomes
  // undefined.
  void clobber_memory() { std::memset(data(), kDirty, size()); }
};

struct AllocBlockHeader {
 public:
  using self = AllocBlockHeader;

  AllocBlockHeader(AllocSet aset, Memory mem)
      : aset(aset),
        free_ptr(static_cast<Pointer>(mem.ptr) + HeaderBase<self>::kMinSize),
        end_ptr(static_cast<Pointer>(mem.ptr) + mem.size) {}

  AllocSet aset;      // aset that owns this block
  Pointer free_ptr;   // Start of free space in this block
  Pointer end_ptr;    // End of space in this block
  AllocBlock next{};  // Next block in aset's blocks list
};

// An AllocBlock is the unit of memory that is obtained by aset.c
// from malloc(). It contains one or more AllocChunks, which are
// the units requested by palloc() and freed by pfree(). AllocChunks
// cannot be returned to malloc() individually, instead they are put
// on freelists by pfree() and re-used by the next palloc() that has
// a matching request size.
//
// AllocBlockHeader is the header data for a block --- the usable space
// within the block begins at the next alignment boundary.
struct AllocBlockData : public HeaderBase<AllocBlockHeader> {
 public:
  using Base = HeaderBase<AllocBlockHeader>;

  explicit AllocBlockData(AllocSet aset, Memory mem) : Base({aset, mem}, mem) {}

  Pointer free_ptr() { return this->header().free_ptr; }
  ConstPointer free_ptr() const { return this->header().free_ptr; }
  Pointer end_ptr() { return this->header().end_ptr; }
  ConstPointer end_ptr() const { return this->header().end_ptr; }
  AllocBlock next() { return this->header().next; }
  AllocSet aset() { return this->header().aset; }
  Size size() const { return end_ptr() - base(); }

  // Get available space inside this block.
  Size avail_space() const { return end_ptr() - free_ptr(); }

  void set_free_ptr(Pointer ptr) { this->header().free_ptr = ptr; }
  void set_end_ptr(Pointer ptr) { this->header().end_ptr = ptr; }
  void set_next(AllocBlock next) { this->header().next = next; }

  //  This function performs a memory integrity check by verifying that each
  //  chunk within this block remains uncorrupted.
  void memory_check();

  // The reset operation reverts the free pointer to its initial position.
  void reset() {
    set_free_ptr(static_cast<Pointer>(data()));
    set_next(nullptr);
  }

  // Fetch one chunk with the specified chunk size from this block.
  AllocChunk fetch_chunk(Size chunk_size, Size requested_size) {
    Size required_size = kChunkHdrSz + chunk_size;

    if (avail_space() < required_size) {
      return nullptr;
    }

    Pointer ptr = free_ptr();
    Memory mem{ptr, chunk_size};

    auto chunk = ::new (AllocChunkData)(mem, aset(), requested_size);
    chunk->set_requested_size(requested_size);

    set_free_ptr(ptr + required_size);

    return chunk;
  }
};

struct LinkedBlock {
 public:
  using value_type = AllocBlock;

  AllocBlock head() { return head_; }
  ConstAllocBlock head() const { return head_; }

  Size size() const { return size_; }

  // Enqueue the provided block. If the available space of the given block is
  // greater than that of the current head, it will become the new head.
  // Otherwise, it will be placed after the current head.
  void enqueue(AllocBlock block) {
    if (head_ == nullptr) {
      head_ = block;
    } else {
      if (block->avail_space() > head_->avail_space()) {
        block->set_next(head_);
        head_ = block;
      } else {
        block->set_next(head_->next());
        head_->set_next(block);
      }
    }

    size_++;
  }

  // Remove the specified block and return true if succeed to remove.
  // Otherwise return false.
  bool remove(void* block) {
    AllocBlock prev = nullptr;
    AllocBlock curr = head();

    while (curr != nullptr) {
      if (curr == block) {
        remove(prev, curr);

        return true;
      }

      prev = curr;
      curr = curr->next();
    }

    return false;
  }

  void reset() {
    head_ = nullptr;
    size_ = 0;
  }

  struct Iterator {
   public:
    Iterator operator++(int) {
      assert(block_ != nullptr);

      return {block_->next()};
    }

    Iterator operator++() {
      assert(block_ != nullptr);

      block_ = block_->next();

      return {block_};
    }

    value_type operator*() { return block_; }

    friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
      return lhs.block_ == rhs.block_;
    }

    friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {
      return !(lhs == rhs);
    }

   private:
    friend class LinkedBlock;

    Iterator(value_type block) : block_(block) {}

    value_type block_;
  };

  Iterator begin() { return {head_}; }
  Iterator end() { return {nullptr}; }

 private:
  void remove(AllocBlock prev, AllocBlock target) {
    AllocBlock next = target->next();

    if (prev == nullptr) {
      head_ = next;
    } else {
      prev->set_next(next);
    }

    size_--;
  }

  AllocBlock head_;
  Size size_{};
};

class AllocSetContext : public MemoryContextData {
 public:
  static constexpr int kMinBits = 4;  // Smallest chunk size is 16 bytes
  static constexpr int kNumFreeLists = 10;
  static constexpr int kChunkLimit = (1 << (kNumFreeLists - 1 + kMinBits));
  static constexpr Size kMinBlockSize = 1024;

  AllocSetContext(MemoryContext parent, std::string name, Size min_context_size,
                  Size init_block_size, Size max_block_size);

  void* alloc(Size size) override;
  void free(void* ptr) override;
  void* realloc(void* ptr, Size size) override;
  void reset() override;
  void destroy() override;

  // Walk through chunks and check consistency of memory.
  //
  // NOTE: report errors as NOTICE, *not* ERROR or FATAL. Otherwise you'll
  // find yourself in an infinite loop when trouble occurs, because this
  // routine will be entered again when elog cleanup tries to release memory!
  void check() override;
  void stats() override;

 private:
  static int free_index(Size size) {
    int index = 0;

    if (size > 0) {
      size = (size - 1) >> kMinBits;

      while (size != 0) {
        index++;
        size >>= 1;
      }

      assert(index < kNumFreeLists);
    }

    return index;
  }

  AllocChunk alloc_large_chunk(Size size);
  AllocChunk try_alloc_from_freelist(Size size);
  AllocChunk alloc_from_block(Size size);
  void merge_block_remainder_to_chunk(AllocBlock block);
  void return_chunk_to_freelist(AllocChunk chunk);

  void free_large_chunk(AllocChunk chunk);

  LinkedBlock blocks_;
  AllocBlock keeper_;
  AllocChunk freelist_[kNumFreeLists];
  Size init_block_size_;
  Size max_block_size_;
};

}  // namespace rdbms