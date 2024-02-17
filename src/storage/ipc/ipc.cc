#include <cassert>
#include <csignal>
#include <iostream>
#include <vector>

#include "rdbms/storage/ipc.hpp"

#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

#include "rdbms/utils/alloc.hpp"

using namespace rdbms;

int Semaphore::next_key_ = 0;

Semaphore::Semaphore(int nsems, int permission, int start_value,
                     bool remove_on_exit)
    : semid_(kBadSemid), remove_on_exit_(remove_on_exit) {
  semid_ = create_semid(nsems, permission);
  init(nsems, start_value);
  set_marker_at_end(nsems);
}

Semaphore::~Semaphore() {
  if (remove_on_exit_) {
    kill();
  }
}

void Semaphore::kill() {
  assert(is_ok());

  if (Semaphore::semctl(semid_, 0, IPC_RMID) < 0) {
    fprintf(stderr, "%s: semctl(%d, 0, IPC_RMID, ...) failed: %s\n", __func__,
            semid_, strerror(errno));
  }

  semid_ = kBadSemid;
}

void Semaphore::acquire(int semnum, bool interrupt_ok) {
  assert(is_ok());

  int err_status;
  struct sembuf sops;

  sops.sem_op = -1;
  sops.sem_flg = 0;
  sops.sem_num = semnum;

  // Note: if err_status is -1 and errno == EINTR then it means we
  // returned from the operation prematurely because we were sent a
  // signal. So we try and lock the semaphore again.
  //
  // Each time around the loop, we check for a cancel/die interrupt. We
  // assume that if such an interrupt comes in while we are waiting, it
  // will cause the semop() call to exit with errno == EINTR, so that we
  // will be able to service the interrupt (if not in a critical section
  // already).
  //
  // Once we acquire the lock, we do NOT check for an interrupt before
  // returning. The caller needs to be able to record ownership of the
  // lock before any interrupt can be accepted.
  //
  // There is a window of a few instructions between CHECK_FOR_INTERRUPTS
  // and entering the semop() call. If a cancel/die interrupt occurs in
  // that window, we would fail to notice it until after we acquire the
  // lock (or get another interrupt to escape the semop()). We can
  // avoid this problem by temporarily setting ImmediateInterruptOK to
  // true before we do CHECK_FOR_INTERRUPTS; then, a die() interrupt in
  // this interval will execute directly. However, there is a huge
  // pitfall: there is another window of a few instructions after the
  // semop() before we are able to reset ImmediateInterruptOK.  If an
  // interrupt occurs then, we'll lose control, which means that the
  // lock has been acquired but our caller did not get a chance to
  // record the fact. Therefore, we only set ImmediateInterruptOK if the
  // caller tells us it's OK to do so, ie, the caller does not need to
  // record acquiring the lock. (This is currently true for lockmanager
  // locks, since the process that granted us the lock did all the
  // necessary state updates. It's not true for SysV semaphores used to
  // emulate spinlocks --- but our performance on such platforms is so
  // horrible anyway that I'm not going to worry too much about it.)
  do {
    STORE(g_immediate_interrupt_ok, interrupt_ok);
    CHECK_FOR_INTERRUPTS();
    err_status = semop(semid_, &sops, 1);
    STORE(g_immediate_interrupt_ok, false);
  } while (err_status == -1 && errno == EINTR);

  if (err_status == -1) {
    fprintf(stderr, "%s: semop(id=%d) failed: %s\n", __func__, semid_,
            strerror(errno));

    ExitManager::proc_exit(1);
  }
}

// ERRORS
// [EAGAIN] The semaphore's value would result in the process being put to sleep
//          and IPC_NOWAIT is specified.
bool Semaphore::try_acquire(int semnum) {
  assert(is_ok());

  int err_status;
  struct sembuf sops;

  sops.sem_op = -1;
  sops.sem_flg = IPC_NOWAIT;
  sops.sem_num = semnum;

  do {
    err_status = semop(semid_, &sops, 1);
  } while (err_status == -1 && errno == EINTR);

  if (err_status == -1) {
    if (errno == EAGAIN) {
      return false;
    }

    // Otherwise we got trouble.
    fprintf(stderr, "%s: semop(id=%d) failed: %s\n", __func__, semid_,
            strerror(errno));
    std::exit(1);
  }

  return true;
}

// For each element in sops, sem_op and sem_flg determine an operation to be
// performed on semaphore number sem_num in the set.  The values SEM_UNDO
// and IPC_NOWAIT may be OR'ed into the sem_flg member in order to modify the
// behavior of the given operation.
//
// The operation performed depends as follows on the value of sem_op:
// 1 When sem_op is positive and the process has alter permission, the
//   semaphore's value is incremented by sem_op's value.  If SEM_UNDO is
//   specified, the semaphore's adjust on exit value is decremented by sem_op's
//   value. A positive value for sem_op generally corresponds to a process
//   releasing a resource associated with the semaphore.
//
// 2 The behavior when sem_op is negative and the process has alter permission,
//   depends on the current value of the semaphore:
//
//  2.1 If the current value of the semaphore is greater than or equal to the
//      absolute value of sem_op, then the value is decremented by the absolute
//      value of sem_op. If SEM_UNDO is specified, the semaphore's adjust on
//      exit value is incremented by the absolute value of sem_op.
//
//  2.2 If the current value of the semaphore is less than the absolute value of
//      sem_op, one of the following happens:
//
//      2.2.1 If IPC_NOWAIT was specified, then semop() returns immediately with
//            a return value of EAGAIN.
//
//      2.2.2 Otherwise, the calling process is put to sleep until one of the
//            following conditions is satisfied:
//
//          2.2.2.1 Some other process removes the semaphore with the IPC_RMID
//                  option of semctl(2). In this case, semop() returns
//                  immediately with a return value of EIDRM.
//
//          2.2.2.2 The process receives a signal that is to be caught. In this
//                  case, the process will resume execution as defined by
//                  sigaction(2).
//
//          2.2.2.3 The semaphore's value is greater than or equal to the
//                  absolute value of sem_op. When this condition becomes true,
//                  the semaphore's value is decremented by the absolute value
//                  of sem_op, the semaphore's adjust on exit value is
//                  incremented by the absolute value of sem_op.
// A negative value for sem_op generally means that a process is waiting for a
// resource to become available.
//
// 3 When sem_op is zero and the process has read permission, one of the
//   following will occur:
//
//  3.1 If the current value of the semaphore is equal to zero then semop() can
//      return immediately.
//
//  3.2 If IPC_NOWAIT was specified, then semop() returns immediately with a
//      return value of EAGAIN.
//
//  3.3 Otherwise, the calling process is put to sleep until one of the
//      following conditions is satisfied:
//
//      3.3.1 Some other process removes the semaphore with the IPC_RMID option
//            of semctl(2). In this case, semop() returns immediately with a
//            return value of EIDRM.
//
//      3.3.2 The process receives a signal that is to be caught.  In this case,
//            the process will resume execution as defined by sigaction(2).
//
//      3.3.3 The semaphore's value becomes zero.
//
// For each semaphore a process has in use, the kernel maintains an "adjust on
// exit" value, as alluded to earlier. When a process exits, either voluntarily
// or involuntarily, the adjust on exit value for each semaphore is added to the
// semaphore's value. This can be used to insure that a resource is released if
// a process terminates unexpectedly.
void Semaphore::release(int semnum, std::ptrdiff_t update) {
  assert(is_ok());

  struct sembuf sops;
  int err_status;

  sops.sem_op = update;
  sops.sem_flg = 0;
  sops.sem_num = semnum;

  // Note: if err_status is -1 and errno == EINTR then it means we
  // returned from the operation prematurely because we were sent a
  // signal. So we try and unlock the semaphore again. Not clear this
  // can really happen, but might as well cope.
  do {
    err_status = semop(semid_, &sops, 1);
  } while (err_status == -1 && errno == EINTR);

  if (err_status == -1) {
    fprintf(stderr, "%s: semop(id=%d) failed: %s\n", __func__, semid_,
            strerror(errno));
    ExitManager::proc_exit(1);
  }
}

bool Semaphore::discover_and_remove_legacy_semaphore(key_t key, int nsems) {
  // See if it looks to be leftover from a dead Postgres process.
  int semid = ::semget(key, nsems, 0);

  if (semid < 0) {
    return false;
  }

  int semnum = nsems - 1;
  int value = Semaphore::semctl(semid, semnum, GETVAL);

  if (value != kPGSemaMagic) {
    return false;
  }

  // If the creator PID is my own PID or does not belong to any
  // extant process, it's safe to zap it.
  pid_t creator = Semaphore::semctl(semid, semnum, GETPID);

  if (creator <= 0) {
    return false;
  }

  if (creator != getpid()) {
    // kill(pid, sig)
    // The kill() function sends the signal specified by sig to pid, a process
    // or a group of processes.
    // Typically, sig will be one of the signals specified in sigaction(2).
    // A value of 0, however, will cause error checking to the performed (with
    // no signal being sent). This can be used to check the validity of pid.
    //
    // For a process to have permission to send a signal to a process designated
    // by pid, the real or effective user ID of the receiving process must match
    // that of the sending process or the user must have appropriate privileges
    // (such as given by a set-user-ID program or the user is the super-user).
    // A single exception is the signal SIGCONT, which may always be sent to any
    // descendant of the current process.
    if (::kill(creator, 0) == 0 || errno != ESRCH) {
      return false;
    }
  }

  // The sema set appears to be from a dead Postgres process, or
  // from a previous cycle of life in this same process. Zap it, if
  // possible. This probably shouldn't fail, but if it does, assume
  // the sema set belongs to someone else after all, and continue
  // quietly.
  if (Semaphore::semctl(semid, 0, IPC_RMID) < 0) {
    return false;
  }

  return true;
}

int Semaphore::create_semid(int nsems, int permission) {
  int semid;

  while (true) {
    next_key_++;
    semid = try_create_semid(next_key_, nsems + 1, permission);

    if (semid >= 0) {
      break;
    }
  }

  return semid;
}

// ERRORS
// [EACCES]: Access permission failure.
// [EEXIST]: IPC_CREAT and IPC_EXCL were specified, and a semaphore
//           set corresponding to key already exists.
// [EIDRM]:  Identifier removed (Won't happend based on the manual)
int Semaphore::try_create_semid(key_t key, int nsems, int permission) {
  assert(nsems > 0 && nsems <= kMaxSema);

  int semflg = permission | IPC_CREAT | IPC_EXCL;
  int semid = ::semget(key, nsems, semflg);

  if (semid < 0) {
    // Fail quietly if error indicates a collision with existing set.
    // One would expect EEXIST, given that we said IPC_EXCL, but
    // perhaps we could get a permission violation instead? Also,
    // EIDRM might occur if an old set is slated for destruction but
    // not gone yet.
    if (errno == EEXIST || errno == EACCES) {
      if (!discover_and_remove_legacy_semaphore(key, nsems)) {
        return kBadSemid;
      }

      return try_create_semid(key, nsems, permission);
    }

    fprintf(stderr, "%s: semget(key=%d, num=%d, 0%o) failed: %s\n", __func__,
            key, nsems, semflg, strerror(errno));

    if (errno == ENOSPC) {
      fprintf(
          stderr,
          "\nThis error does *not* mean that you have run out of disk "
          "space.\n\n"
          "It occurs either because system limit for the maximum number of\n"
          "semaphore sets (SEMMNI), or the system wide maximum number of\n"
          "semaphores (SEMMNS), would be exceeded.  You need to raise the\n"
          "respective kernel parameter. Look into the PostgreSQL "
          "documentation\n"
          "for details.\n\n");
    }

    ExitManager::proc_exit(1);
  }

  return semid;
}

void Semaphore::init(int nsems, int start_value) {
  union semun semun;
  std::vector<u_short> init_values(nsems, start_value);

  semun.array = init_values.data();

  if (Semaphore::semctl(semid_, 0, SETALL, semun) < 0) {
    fprintf(stderr, "%s: semctl(id=%d, 0, SETALL, ...) failed: %s\n", __func__,
            semid_, strerror(errno));

    if (errno == ERANGE) {
      fprintf(stderr,
              "You possibly need to raise your kernel's SEMVMX value to be at "
              "least %d. Look into the PostgreSQL documentation for details.\n",
              start_value);
    }

    kill();
    ExitManager::proc_exit(1);
  }
}

void Semaphore::set_marker_at_end(int semnum) {
  // OK, we created a new sema set. Mark it as created by this process.
  // We do this by setting the spare semaphore to PGSemaMagic-1 and then
  // incrementing it with semop(). That leaves it with value
  // PGSemaMagic and sempid referencing this process.
  union semun semun;
  semun.val = kPGSemaMagic - 1;

  if (Semaphore::semctl(semid_, semnum, SETVAL, semun) < 0) {
    fprintf(stderr, "%s: semctl(id=%d, %d, SETVAL, %d) failed: %s\n", __func__,
            semid_, semnum, kPGSemaMagic - 1, strerror(errno));

    if (errno == ERANGE) {
      fprintf(stderr,
              "You possibly need to raise your kernel's SEMVMX value to be at "
              "least\n"
              "%d.  Look into the PostgreSQL documentation for details.\n",
              kPGSemaMagic);
    }

    kill();
    ExitManager::proc_exit(1);
  }

  release(semnum);
}

// ======================================================================
// Shared Memory
// ======================================================================
int SharedMemory::next_key_ = 0;

SharedMemory::SharedMemory(Size size, int permission, bool is_private)
    : is_private_(is_private), shmid_(kBadShmid) {
  void* ptr;

  if (is_private) {
    ptr = create_private_memory(size);
  } else {
    auto res = create_shared_memory(size, permission);
    shmid_ = res.first;
    ptr = res.second;
  }

  shmaddr_ = ::new (ptr) PGShmemHeader(kPGShmemMagic, getpid(), size,
                                       MAX_ALIGN(sizeof(PGShmemHeader)));
}

SharedMemory::~SharedMemory() {
  if (is_private_) {
    remove_private_memory(shmaddr_);
  } else {
    detach_shared_memory(shmaddr_);
    remove_shared_memory(shmid_);
  }
}

void* SharedMemory::create_private_memory(Size size) {
  auto mem = MemoryPool::allocate(size);
  auto ptr = mem.ptr;

  ExitManager::on_shmem_exit(remove_private_memory, ptr);

  return ptr;
}

void SharedMemory::remove_private_memory(void* ptr) {
  MemoryPool::deallocate(ptr);
}

std::pair<int, void*> SharedMemory::create_shared_memory(Size size,
                                                         int permission) {
  std::pair<int, void*> res;

  while (true) {
    next_key_++;
    res = try_create_shared_memory(next_key_, size, permission);

    if (res.first != kBadShmid) {
      break;
    }
  }

  return res;
}

std::pair<int, void*> SharedMemory::try_create_shared_memory(key_t key,
                                                             std::size_t size,
                                                             int permission) {
  int shmflg = IPC_CREAT | IPC_EXCL | permission;
  int shmid = ::shmget(key, size, shmflg);

  if (shmid < 0) {
    // Fail quietly if error indicates a collision with existing
    // segment. One would expect EEXIST, given that we said IPC_EXCL,
    // but perhaps we could get a permission violation instead?
    if (errno == EEXIST || errno == EACCES) {
      // If removal of legacy shared memory fails, move on to the next key.
      // Otherwise, attempt to reuse this key.
      if (!discover_and_remove_legacy_shmem(key)) {
        return {kBadShmid, nullptr};
      }

      return try_create_shared_memory(key, size, permission);
    }

    fprintf(stderr, "%s: shmget(key=%d, size=%ld, 0%o) failed: %s\n", __func__,
            key, size, shmflg, strerror(errno));

    if (errno == EINVAL) {
      fprintf(
          stderr,
          "This error can be caused by one of three things:\n"
          "1. The maximum size for shared memory segments on your system was\n"
          "   exceeded. You need to raise the SHMMAX parameter in your\n"
          "   kernel to be at least %ld bytes.\n"
          "2. The requested shared memory segment was too small for your\n"
          "   system. You need to lower the SHMMIN parameter in your\n"
          "   kernel.\n"
          "3. The requested shared memory segment already exists but is of\n"
          "   the wrong size. This is most likely the case if an old version\n"
          "   of PostgreSQL crashed and didn't clean up.  The `ipcclean'\n"
          "   utility can be used to remedy this.\n"
          "The PostgreSQL Administrator's Guide contains more information "
          "about shared memory configuration.\n",
          size);
    } else if (errno == ENOSPC) {
      fprintf(
          stderr,
          "\nThis error does *not* mean that you have run out of disk "
          "space.\n\n"
          "It occurs either if all available shared memory ids have been "
          "taken,\n"
          "in which case you need to raise the SHMMNI parameter in your "
          "kernel,\n"
          "or because the system's overall limit for shared memory has been\n"
          "reached.  The PostgreSQL Administrator's Guide contains more\n"
          "information about shared memory configuration.\n\n");
    }

    ExitManager::proc_exit(1);
  }

  // Register on-exit routine to delete the new segment.
  ExitManager::on_shmem_exit(remove_shared_memory, shmid);

  void* shmaddr = ::shmat(shmid, nullptr, 0);

  if (shmaddr == reinterpret_cast<void*>(-1)) {
    fprintf(stderr, "%s: shmat(id=%d) failed: %s\n", __func__, shmid,
            strerror(errno));
    ExitManager::proc_exit(1);
  }

  // Register on-exit routine to detach new segment before deleting.
  ExitManager::on_shmem_exit(detach_shared_memory, shmaddr);

  // TODO(gc): fix this.
  // RecordSharedMemoryInLockFile(memKey, shmid);

  return {shmid, shmaddr};
}

void SharedMemory::remove_shared_memory(int shmid) {
  if (::shmctl(shmid, IPC_RMID, nullptr) < 0) {
    fprintf(stderr, "%s: shmctl(%d, %d, 0) failed: %s\n", __func__, shmid,
            IPC_RMID, strerror(errno));
  }
}

void SharedMemory::detach_shared_memory(const void* shmaddr) {
  if (::shmdt(shmaddr) < 0) {
    fprintf(stderr, "%s: shmdt(%p) failed: %s\n", __func__, shmaddr,
            strerror(errno));
  }
}

// Precondition: [EACCES] or [EEXIST]
bool SharedMemory::discover_and_remove_legacy_shmem(key_t key) {
  int shmid = ::shmget(key, sizeof(PGShmemHeader), 0);

  if (shmid < 0) {
    return false;
  }

  void* shmaddr = ::shmat(shmid, nullptr, 0);

  if (shmaddr == reinterpret_cast<void*>(-1)) {
    return false;
  }

  auto header = static_cast<PGShmemHeader*>(shmaddr);

  if (header->magic != kPGShmemMagic) {
    detach_shared_memory(shmaddr);

    return false;
  }

  // If the creator PID is my own PID or does not belong to any
  // extant process, it's safe to zap it.
  if (auto cpid = header->creator_pid; cpid != getpid()) {
    if (::kill(cpid, 0) == 0 || errno != ESRCH) {
      detach_shared_memory(shmaddr);

      return false;
    }
  }

  // The segment appears to be from a dead Postgres process, or from
  // a previous cycle of life in this same process.  Zap it, if
  // possible. This probably shouldn't fail, but if it does, assume
  // the segment belongs to someone else after all, and continue
  // quietly.
  detach_shared_memory(shmaddr);

  if (::shmctl(shmid, IPC_RMID, nullptr) < 0) {
    return false;
  }

  return true;
}