#include <cstdio>
#include <cstdlib>

#include "rdbms/storage/slock.hpp"

#include <sys/select.h>

#include "rdbms/utils/globals.hpp"

using namespace rdbms;

namespace rdbms {

// Each time we busy spin we select the next element of this array as the
// number of microseconds to wait. This accomplishes pseudo random back-off.
//
// Note that on most platforms, specified values will be rounded up to the
// next multiple of a clock tick, which is often ten milliseconds (10000).
// So, we are being way overoptimistic to assume that these different values
// are really different, other than the last.  But there are a few platforms
// with better-than-usual timekeeping, and on these we will get pretty good
// pseudo-random behavior.
//
// Total time to cycle through all 20 entries will be at least 100 msec,
// more commonly (10 msec resolution) 220 msec, and on some platforms
// as much as 420 msec (when the remainder of the current tick cycle is
// ignored in deciding when to time out, as on FreeBSD and older Linuxen).
// We use the 100msec figure to figure max_spins, so actual timeouts may
// be as much as four times the nominal value, but will never be less.

#define S_NSPIN_CYCLE   20
#define AVG_SPINCYCLE   5000  // Average entry in microsec: 100ms / 20
#define DEFAULT_TIMEOUT (100 * 1000000)  // Default timeout: 100 sec

static void slock_stuck(LwLock* lock, const char* file, int lineno) {
  fprintf(stderr, "FATAL: %s(%p) at %s:%d, stuck spinlock. Aborting.\n",
          __func__, lock, file, lineno);
  abort();
}

// Sleep a pseudo-random amount of time, check for timeout
//
// The 'timeout' is given in microsec, or may be 0 for "infinity".	Note
// that this will be a lower bound (a fairly loose lower bound, on most
// platforms).
//
// 'microsec' is the number of microsec to delay per loop.	Normally
// 'microsec' is 0, specifying to use the next s_spincycle[] value.
// Some callers may pass a nonzero interval, specifying to use exactly that
// delay value rather than a pseudo-random delay.
static void slock_sleep(unsigned int spins, int timeout, int micro_sec,
                        LwLock* lock, const char* filename, int lineno) {
  static int spincycle[S_NSPIN_CYCLE] = {
      1,    10,    100,  1000, 10000, 1000, 1000,  1000, 10000, 1000,
      1000, 10000, 1000, 1000, 10000, 1000, 10000, 1000, 10000, 30000};

  struct timeval delay;

  if (micro_sec > 0) {
    delay.tv_sec = micro_sec / 1000000;
    delay.tv_usec = micro_sec % 1000000;
  } else {
    delay.tv_sec = 0;
    delay.tv_usec = spincycle[spins % S_NSPIN_CYCLE];
    micro_sec = AVG_SPINCYCLE;
  }

  if (timeout > 0) {
    unsigned int max_spins = timeout / micro_sec;

    if (spins > max_spins) {
      slock_stuck(lock, filename, lineno);
    }
  }

  (void)select(0, nullptr, nullptr, nullptr, &delay);
}

void slock(LwLock* lock, const char* filename, int lineno) {
  unsigned int spins = 0;

  // If you are thinking of changing this code, be careful.  This same
  // loop logic is used in other places that call TAS() directly.
  //
  // While waiting for a lock, we check for cancel/die interrupts (which is
  // a no-op if we are inside a critical section).  The interrupt check
  // can be omitted in places that know they are inside a critical
  // section. Note that an interrupt must NOT be accepted after
  // acquiring the lock.
  while (tas(lock)) {
    slock_sleep(spins++, DEFAULT_TIMEOUT, 0, lock, filename, lineno);
    CHECK_FOR_INTERRUPTS();
  }
}

}  // namespace rdbms