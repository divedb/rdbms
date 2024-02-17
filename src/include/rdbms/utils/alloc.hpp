#pragma once

#include "rdbms/postgres.hpp"

namespace rdbms {

struct Memory {
  void* ptr;
  Size size;
};

// A simple memory wrapper for malloc and free.
// The reason to add this:
// 1. Add some protection when memory allocation failed.
// 2. This allocator could be replaced in the future.
class MemoryPool {
 public:
  static Memory allocate(Size nbytes);
  static Memory reallocate(void* ptr, Size nbytes);
  static void deallocate(void* ptr);
  static Size bytes_allocated() { return bytes_allocated_; }

 private:
  static Size recommend_size(Size nbytes);

  static Size bytes_allocated_;
};

}  // namespace rdbms