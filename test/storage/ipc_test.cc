#include <random>
#include <thread>
#include <vector>

#include "rdbms/storage/ipc.hpp"

#include <gtest/gtest.h>

using namespace rdbms;

struct MockExiter {
 public:
  void operator()(int code) {}
};

TEST(ExitHandler, AddOne) {
  int n = 10;
  int x = 0;

  auto add_one = [](int* x) { *x += 1; };

  for (int i = 0; i < n; i++) {
    ExitManager::on_proc_exit(add_one, &x);
    ExitManager::on_shmem_exit(add_one, &x);
  }

  ExitManager::proc_exit<MockExiter>(0);

  EXPECT_EQ(2 * n, x);
}

int random_delay() {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 100);
  int delay_ms = dis(gen);

  return delay_ms;
}

struct Task {
  int loops;
  int* x;
  BinarySemaphore* sem;
};

void routine(Task task) {
  int loops = task.loops;
  int* x = task.x;
  BinarySemaphore* sem = task.sem;

  for (int i = 0; i < loops; i++) {
    sem->acquire();
    *x = *x + 1;
    sem->release();

    std::this_thread::sleep_for(std::chrono::milliseconds(random_delay()));
  }
}

TEST(BinarySemaphore, LockAndUnLock) {
  int x = 0;
  int loops = 11;
  BinarySemaphore sem;

  int nthreads = 32;
  Task task{loops, &x, &sem};
  std::vector<std::thread> thread_group;

  sem.release();

  for (int i = 0; i < nthreads; i++) {
    thread_group.emplace_back(routine, task);
  }

  for (auto&& thread : thread_group) {
    thread.join();
  }

  EXPECT_EQ(loops * nthreads, x);
}

TEST(Semaphore, DiscoverAndRemoveLegacy) {
  int nsems = 1;
  int start_value = 0;
  int pipefd[2];
  int permission = 0600;

  assert(pipe(pipefd) != -1);

  pid_t pid = fork();

  if (pid == 0) {
    bool remove_on_exit = false;

    // Dont remove this semaphore.
    // Let parent process discover this legacy semaphore.
    Semaphore sema(nsems, permission, start_value, remove_on_exit);
    EXPECT_TRUE(sema.is_ok());

    int key = sema.key();

    // Close read end and write key to pipe.
    // Let parent process receive this key.
    close(pipefd[0]);
    write(pipefd[1], &key, sizeof(key));
    close(pipefd[1]);

    ExitManager::proc_exit(0);
  } else if (pid > 0) {
    int key = -1;
    read(pipefd[0], &key, sizeof(key));
    wait(nullptr);

    Semaphore sema(nsems, permission, start_value, true);
    EXPECT_TRUE(sema.is_ok());

    // Expect the key is reused.
    EXPECT_EQ(key, sema.key());
    close(pipefd[0]);
    close(pipefd[1]);
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}