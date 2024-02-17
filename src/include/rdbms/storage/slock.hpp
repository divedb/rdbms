#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace rdbms {

#define S_LOCK(lock)                     \
  do {                                   \
    if (tas(lock)) {                     \
      slock((lock), __FILE__, __LINE__); \
    }                                    \
  } while (0)

#define S_UNLOCK(lock)    (*(lock) = 0)
#define S_LOCK_FREE(lock) (*(lock) = 0)
#define S_LOCK_INIT(lock) S_UNLOCK(lock)

using LwLock = int;

// Exchanges the contents of the destination (first) and source (second)
// operands. The operands can be two general-purpose registers or a register and
// a memory location. If a memory operand is referenced, the processor's locking
// protocol is automatically implemented for the duration of the exchange
// operation, regardless of the presence or absence of the LOCK prefix or of the
// value of the IOPL. (See the LOCK prefix description in this chapter for more
// information on the locking protocol.)
static inline int tas(LwLock* lock) {
  int res = 1;

  asm volatile(
      "lock\n"
      "xchg %0, %1\n"
      : "=q"(res), "=m"(*lock)
      : "0"(res));

  return res;
}

void slock(LwLock* lock, const char* filename, int lineno);

class TasLock {
 public:
  static const char* name() { return "TasLock"; }

  TasLock() { S_LOCK_INIT(&lock_); }

  constexpr void acquire() { S_LOCK(&lock_); }
  constexpr void release() { S_UNLOCK(&lock_); }

 private:
  LwLock lock_;
};

class MutexLock {
 public:
  static const char* name() { return "MutexLock"; }

  constexpr void acquire() { mtx_.lock(); }
  constexpr void release() { mtx_.unlock(); }

 private:
  std::mutex mtx_;
};

class AtomicLock {
 public:
  static const char* name() { return "AtomicLock"; }

  AtomicLock() {}

  constexpr void acquire() {
    while (flag_.test_and_set(std::memory_order_acquire)) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  constexpr void release() { flag_.clear(std::memory_order_release); }

 private:
  std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

}  // namespace rdbms
