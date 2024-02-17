#include <cstddef>
#include <cstdlib>

#include "rdbms/utils/alloc.hpp"

using namespace rdbms;

#define HEADER_ALIGN()    MAX_ALIGN(sizeof(Size))
#define HEADER_SIZE(base) *reinterpret_cast<Size*>(base)
#define GET_POINTER(base) (static_cast<Pointer>(base) + HEADER_ALIGN())
#define GET_BASE(ptr)     (static_cast<Pointer>(ptr) - HEADER_ALIGN())

Size MemoryPool::bytes_allocated_ = 0;

Memory MemoryPool::allocate(std::size_t nbytes) {
  Size size = recommend_size(nbytes);
  void* base = ::malloc(size);

  // TODO(gc): add log
  if (base == nullptr) {
    return {nullptr, 0};
  }

  bytes_allocated_ += nbytes;
  HEADER_SIZE(base) = nbytes;

  return {GET_POINTER(base), nbytes};
}

// If ptr is NULL, realloc() is identical to a call
// to malloc() for size bytes. If size is zero and ptr is not NULL, a new,
// minimum sized object is allocated and the original object is freed. When
// extending a region allocated with calloc(3), realloc(3) does not
// guarantee that the additional memory is also zero-filled.
Memory MemoryPool::reallocate(void* ptr, std::size_t nbytes) {
  if (ptr == nullptr) {
    return allocate(nbytes);
  }

  Pointer base = GET_BASE(ptr);
  Size size = recommend_size(nbytes);
  Size old_size = HEADER_SIZE(base);

  bytes_allocated_ += (nbytes - old_size);

  return {::realloc(base, size), nbytes};
}

void MemoryPool::deallocate(void* ptr) {
  Pointer base = GET_BASE(ptr);
  bytes_allocated_ -= HEADER_SIZE(base);

  ::free(base);
}

Size MemoryPool::recommend_size(Size nbytes) { return HEADER_ALIGN() + nbytes; }
