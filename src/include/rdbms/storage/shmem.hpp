#pragma once

#include "rdbms/storage/ipc.hpp"
#include "rdbms/utils/hsearch.hpp"

namespace rdbms {

class ShmemAllocator {
 public:
  // Allocate max-aligned chunk from shared memory.
  // Assumes ShmemLock and ShmemSegHdr are initialized.
  //
  void* alloc(Size size);

 private:
  SharedMemory shared_mem_;
};

}  // namespace rdbms