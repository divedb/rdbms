#pragma once

#include "rdbms/storage/ipc.hpp"
#include "rdbms/storage/slock.hpp"

namespace rdbms {

using SpinLock = int;

enum LockId {
  kBufMgrLockId,
  kOidGenLockId,
  kXidGenLockId,
  kCtrlFileLockId,
  kShmemLockId,
  kShmemIndexLockId,
  kLockMgrLockId,
  kSinValLockId,
  kProcStructLockId,
  kMaxSpins
};

class SpinManager {
 public:
  SpinManager();

  void acquire(LockId lockid);
  void release(LockId lockid);

 private:
};

}  // namespace rdbms