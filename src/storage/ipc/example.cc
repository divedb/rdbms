#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>  // ftok
#include <sys/shm.h>
#include <unistd.h>

static void print_perm(struct ipc_perm* perm) {
  printf("Creator uid: %d\n", perm->cuid);
  printf("Creator gid: %d\n", perm->cgid);
  printf("Owner uid: %d\n", perm->uid);
  printf("Owner gid: %d\n", perm->gid);
  printf("Mode: %o\n", perm->mode);
  printf("\n");
}

static void print_shared_memory(int shmid) {
  struct shmid_ds sops;

  int res = shmctl(shmid, IPC_STAT, &sops);

  if (res == -1) {
    fprintf(stderr, "shmctl: %s\n", strerror(errno));
    return;
  }

  print_perm(&sops.shm_perm);
  printf("Segment size = %ld\n", sops.shm_segsz);
  printf("PID of last shm op: %d\n", sops.shm_lpid);
  printf("PID of creator: %d\n", sops.shm_cpid);
  printf("# of current attaches: %d\n", sops.shm_nattch);
  printf("Last shmat() time: %ld\n", sops.shm_atime);
  printf("Last shmdt() time: %ld\n", sops.shm_dtime);
  printf("Last shmctl() time: %ld\n", sops.shm_ctime);
  printf("\n");
}

int main() {
  key_t key = ftok("/tmp", 0);
  if (key == -1) {
    fprintf(stderr, "ftok: %s\n", strerror(errno));

    exit(1);
  }

  // 0400权限shmat是permission denied
  // 0200权限shmctl和shmat都是permission denied
  int shmid = shmget(key, 8192, IPC_CREAT | 0600);

  if (shmid == -1) {
    fprintf(stderr, "shmget: %s\n", strerror(errno));

    exit(1);
  } else {
    printf("Shared memory id: %d\n", shmid);
  }

  printf("================================================\n");
  printf("Create shared memory.\n");
  printf("================================================\n");
  print_shared_memory(shmid);

  sleep(3);
  void* mem = shmat(shmid, 0, 0);

  if (mem == (void*)(-1)) {
    fprintf(stderr, "shmat: %s\n", strerror(errno));

    exit(1);
  }

  printf("================================================\n");
  printf("Attach shared memory.\n");
  printf("================================================\n");
  print_shared_memory(shmid);

  *(int*)mem = 1;

  int res = shmdt(mem);

  if (res == -1) {
    fprintf(stderr, "shmdt: %s\n", strerror(errno));

    exit(1);
  } else {
    printf("Succeed to detach.\n");
  }
}