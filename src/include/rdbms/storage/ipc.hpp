#pragma once

#include <array>
#include <cstdlib>
#include <functional>

#include <sys/sem.h>

#include "rdbms/postgres.hpp"
#include "rdbms/utils/globals.hpp"

namespace rdbms {

template <Size MaxOnExits>
struct ExitHandler {
 public:
  using Handle = std::function<void()>;

  template <typename Func, typename... Args>
  static void on_exit(Func&& func, Args&&... args) {
    // TODO(gc): add log message.
    if (index_ == MaxOnExits) {
      return;
    }

    handlers_[index_++] = [func = std::forward<Func>(func), args...]() mutable {
      func(args...);
    };
  }

  static void reset() { index_ = 0; }

  // Execute the registered function in reverse order.
  static void exit() {
    while (--index_ >= 0) {
      handlers_[index_]();
    }
  }

 private:
  static int index_;
  static std::array<Handle, MaxOnExits> handlers_;
};

template <Size MaxOnExits>
int ExitHandler<MaxOnExits>::index_ = 0;

template <Size MaxOnExits>
std::array<typename ExitHandler<MaxOnExits>::Handle, MaxOnExits>
    ExitHandler<MaxOnExits>::handlers_;

struct DefaultExiter {
  void operator()(int code) const { std::exit(code); }
};

class ExitManager {
 public:
  static constexpr int kMaxProcExits = 64;
  static constexpr int kMaxShmemExits = 32;

  using ProcExitHandler = ExitHandler<kMaxProcExits>;
  using ShmemExitHandler = ExitHandler<kMaxShmemExits>;

  template <typename Func, typename... Args>
  static void on_proc_exit(Func&& func, Args&&... args) {
    ProcExitHandler::on_exit(std::forward<Func>(func),
                             std::forward<Args>(args)...);
  }

  template <typename Func, typename... Args>
  static void on_shmem_exit(Func&& func, Args&&... args) {
    ShmemExitHandler::on_exit(std::forward<Func>(func),
                              std::forward<Args>(args)...);
  }

  static void shmem_exit(int code) {
    IGNORE(code);
    ShmemExitHandler::exit();
  }

  template <typename Exiter = DefaultExiter>
  static void proc_exit(int code, Exiter exiter = Exiter{}) {
    reset_global_vars();

    shmem_exit(code);
    ProcExitHandler::exit();
    exiter(code);
  }

  static void on_exit_reset() {
    ProcExitHandler::reset();
    ShmemExitHandler::reset();
  }

 private:
  static void reset_global_vars() {
    // Once we set this flag, we are committed to exit. Any elog() will
    // NOT send control back to the main loop, but right back here.
    g_proc_exit_inprogress = true;

    // Forget any pending cancel or die requests; we're doing our best to
    // close up shop already. Note that the signal handlers will not set
    // these flags again, now that proc_exit_inprogress is set.
    STORE(g_interrupt_pending, false);
    STORE(g_proc_die_pending, false);
    STORE(g_query_cancel_pending, false);

    // And let's just make *sure* we're not interrupted.
    STORE(g_immediate_interrupt_ok, false);
    STORE(g_interrupt_hold_off_count, 1);
    STORE(g_crit_section_count, 0);
  }
};

class Semaphore {
 public:
  static constexpr int kPGSemaMagic = 537;

  // Create a semaphore set with the given number of useful semaphores
  // (an additional sema is actually allocated to serve as identifier).
  // Dead Postgres sema sets are recycled if found, but we do not fail
  // upon collision with non-Postgres sema sets.
  Semaphore(int nsems, int permission, int start_value,
            bool remove_on_exit = true);

  ~Semaphore();

  // Get semaphore id.
  constexpr int id() const { return semid_; }

  // Check the semaphore is valid.
  constexpr bool is_ok() const { return semid_ != kBadSemid; }

  // Atomically decrements the internal counter by 1 if it is greater than 0;
  // otherwise blocks until it is greater than 0 and can successfully decrement
  // the internal counter.
  void acquire(int semnum, bool interrupt_ok);

  // Tries to atomically decrement the internal counter by 1 if it is greater
  // than 0; no blocking occcurs regardliess.
  bool try_acquire(int semnum);

  // Atomically increments the internal counter by the value of update. Any
  // thread(s) waiting for the counter to be greater than 0, such as due to
  // being blocked in the acquire, will subsequently be unblocked.
  void release(int semnum, std::ptrdiff_t update = 1);

  // Get unique key which is used to generate semaphore id.
  // The reason to do this:
  // For MacOS, semget generate different semphore id for child and parent
  // processes though the key is the same.
  static key_t key() { return next_key_; }

 private:
  // Find next available semaphore id.
  static int create_semid(int nsems, int permission);
  static int try_create_semid(key_t key, int nsems, int permission);

  // Return true if succeed to find and remove legacy semaphores.
  // Otherwise return false.
  static bool discover_and_remove_legacy_semaphore(int semid, int nsems);

  template <typename... Args>
  static int semctl(int semid, int semnum, int cmd, Args&&... args) {
    return ::semctl(semid, semnum, cmd, std::forward<Args>(args)...);
  }

  // Initialize new semas to specified start value.
  void init(int nsems, int start_value);

  void set_marker_at_end(int semnum);

  // Remove this semaphore.
  void kill();

  static int next_key_;
  static constexpr int kBadSemid = -1;
  static constexpr int kMaxSema = 32;

  int semid_;
  bool remove_on_exit_;
};

class BinarySemaphore {
 public:
  BinarySemaphore() : sem_(1, 0666, 0) {}

  // Atomically decrements the internal counter by 1 if it is greater than 0;
  // otherwise blocks until it is greater than 0 and can successfully decrement
  // the internal counter.
  void acquire() { sem_.acquire(0, false); }

  // Tries to atomically decrement the internal counter by 1 if it is greater
  // than 0; no blocking occcurs regardliess.
  bool try_acquire() { return sem_.try_acquire(0); }

  // Atomically increments the internal counter by the value of update. Any
  // thread(s) waiting for the counter to be greater than 0, such as due to
  // being blocked in the acquire, will subsequently be unblocked.
  void release(std::ptrdiff_t update = 1) { sem_.release(0, update); }

  constexpr bool is_ok() const { return sem_.is_ok(); }

 private:
  Semaphore sem_;
};

// The shmctl() system call performs some control operations on the shared
// memory area specified by shmid. Each shared memory segment has a data
// structure associated with it, parts of which may be altered by shmctl()
// and parts of which determine the actions of shmctl(). This structure is
// defined as follows in (sys/shm.h):
// struct shmid_ds {
//  struct ipc_perm shm_perm;       /* operation permissions */
//  size_t          shm_segsz;      /* size of segment in bytes */
//  pid_t           shm_lpid;       /* pid of last shm op */
//  pid_t           shm_cpid;       /* pid of creator */
//  short           shm_nattch;     /* # of current attaches */
//  time_t          shm_atime;      /* last shmat() time */
//  time_t          shm_dtime;      /* last shmdt() time */
//  time_t          shm_ctime;      /* last change by shmctl() */
//  void*           shm_internal;   /* sysv stupidity */
// };
//
// The ipc_perm structure used inside the shmid_ds structure is defined in
// ⟨sys/ipc.h⟩ and looks like this:
//
// struct ipc_perm {
//  uid_t           uid;    /* Owner's user ID */
//  gid_t           gid;    /* Owner's group ID */
//  uid_t           cuid;   /* Creator's user ID */
//  gid_t           cgid;   /* Creator's group ID */
//  mode_t          mode;   /* r/w permission (see chmod(2)) */
//  unsigned short  _seq;   /* Reserved for internal use */
//  key_t           _key;   /* Reserved for internal use */
// };
//
// The operation to be performed by shmctl() is specified in cmd and is one
// of:
// IPC_STAT     Gather information about the shared memory segment and place
//              it in the structure pointed to by buf.
//
// IPC_SET      Set the value of the shm_perm.uid, shm_perm.gid and
//              shm_perm.mode fields in the structure associated with shmid.
//              The values are taken from the corresponding fields in the
//              structure pointed to by buf. This operation can only be
//              executed by the super-user, or a process that has an effective
//              user id equal to either shm_perm.cuid or shm_perm.uid in the
//              data structure associated with the shared memory segment.
//
// IPC_RMID     Remove the shared memory segment specified by shmid and
//              destroy the data associated with it. Only the super-user or a
//              process with an effective uid equal to the shm_perm.cuid or
//              shm_perm.uid values in the data structure associated with the
//              queue can do this.
//
// The read and write permissions on a shared memory identifier are
// determined by the shm_perm.mode field in the same way as is done with
// files (see chmod(2) ), but the effective uid can match either the
// shm_perm.cuid field or the shm_perm.uid field, and the effective gid can
// match either shm_perm.cgid or shm_perm.gid.
//
// RETURN VALUES
//  Upon successful completion, a value of 0 is returned. Otherwise, -1 is
//  returned and the global variable errno is set to indicate the error.
class SharedMemory {
 public:
  SharedMemory(Size size, int permission, bool is_private = false);
  ~SharedMemory();

  constexpr bool is_ok() const { return is_private_ || shmid_ != kBadShmid; }

 private:
  friend class ShmemAllocator;

  struct PGShmemHeader {
    PGShmemHeader(i32 _magic, pid_t _creator_pid, u32 _total_size,
                  u32 _free_offset)
        : magic(_magic),
          creator_pid(_creator_pid),
          total_size(_total_size),
          free_offset(_free_offset) {}

    i32 magic;          // magic # to identify Postgres segments
    pid_t creator_pid;  // PID of creating process
    u32 total_size;     // total size of segment
    u32 free_offset;    // offset to first free space
  };

  static constexpr int kPGShmemMagic = 0x2885750c;

  static void* create_private_memory(Size size);
  static void remove_private_memory(void* ptr);

  static std::pair<int, void*> create_shared_memory(Size size, int permission);
  static std::pair<int, void*> try_create_shared_memory(key_t key, Size size,
                                                        int permission);
  static void detach_shared_memory(const void* shmaddr);
  static void remove_shared_memory(int shmid);
  static bool discover_and_remove_legacy_shmem(key_t key);

  static int next_key_;
  static constexpr int kBadShmid = -1;

  bool is_private_;
  int shmid_;
  PGShmemHeader* shmaddr_;
};

}  // namespace rdbms