#include <iostream>
#include <thread>
#include <vector>

#include "rdbms/storage/slock.hpp"

#include <gtest/gtest.h>

#include "rdbms/utils/timer.hpp"

using namespace rdbms;
using namespace std;

template <typename Lock>
void routine(Lock* lock, int n, unsigned long long* sum) {
  for (int i = 0; i < n; i++) {
    if (i % 2 == 0 || i % 3 == 0) {
      continue;
    }

    lock->acquire();
    *sum += i;
    lock->release();
  }
}

template <typename Lock>
void test_lock_and_unlock() {
  int n = 10101789;
  int nthreads = 15;
  vector<thread> thread_groups;
  unsigned long long sum1 = 0;
  unsigned long long sum2 = 0;

  Lock lock;
  Timer timer;

  for (int i = 0; i < nthreads; i++) {
    thread_groups.push_back(thread{routine<Lock>, &lock, n, &sum1});
  }

  routine(&lock, n, &sum2);

  for (auto&& t : thread_groups) {
    t.join();
  }

  cout << "[" << lock.name() << "] Elapsed Time: " << timer.elapsed() << "ms"
       << endl;

  EXPECT_EQ(sum2 * nthreads, sum1);
}

TEST(SLock, LockAndUnLock) {
  test_lock_and_unlock<TasLock>();
  test_lock_and_unlock<MutexLock>();
  test_lock_and_unlock<AtomicLock>();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}