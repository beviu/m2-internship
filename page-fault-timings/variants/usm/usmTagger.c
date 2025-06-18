#define _GNU_SOURCE

#include <dlfcn.h>

#include <errno.h>
#include <linux/userfaultfd.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/un.h>

#include <arpa/inet.h>

#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include <fcntl.h>

#include <sys/uio.h>

#include <signal.h>
#include <sys/prctl.h>

// #include <libsyscall_intercept_hook_point.h>
#include <errno.h>
#include <syscall.h>

#include <asm/unistd.h>

#define SYS_PAGE_SIZE sysconf(_SC_PAGESIZE)

#define __READ_ONCE(x) (*(const volatile __unqual_scalar_typeof(x) *)&(x))

// volatile unsigned int *usm_bitfield;

#ifdef UINTR // weirdly not gettin' gud mem zone..     | resolved, to test..
void __attribute__((interrupt))
__attribute__((target("general-regs-only", "inline-all-stringops")))
sig_temporizer(struct __uintr_frame *ui_frame, unsigned long long vector) {
  (void)ui_frame;
  (void)vector;
#else
void sig_temporizer(int sig) {
  (void)sig;
#endif
  printf("[Temp./Info.] Active sleepin' (playin')! ... basic doin'.. \n");
init:
  /*while(*usm_bitfield & ((unsigned int)1 << 0)) {
      printf("Yoy%u\n", *usm_bitfield);
      // sleep(1);
      goto init;
  }*/
  for (int i = 0; i < 100000000; i += 2)
    i--;
  printf("[Temp./Info.] Gud, giving hand back!\n");
  // sleep(3);           // yup, temporizer.. ain't even no need to map thingies
  // up then mnumap 'em here and all.. 3 sec.s should be sufficient time... now
  // depends on what 'em say...
}

ssize_t my_uffd(int args) {
  ssize_t ret;
  asm volatile("syscall"
               : "=a"(ret)
               : "0"(__NR_userfaultfd), "D"(args)
               : "rcx", "r11", "memory");
  return ret;
}

/*static int
hook(long syscall_number,
                        long arg0, long arg1,
                        long arg2, long arg3,
                        long arg4, long arg5,
                        long *result) {
    my_uffd(O_CLOEXEC | O_NONBLOCK | UFFD_USM);
    intercept_hook_point=NULL;
        return 1;
}*/

static __attribute__((constructor)) void init(void) {
  int fd, instance = 0;
  if (getenv("USM"))
    instance = strtol(getenv("USM"), (char **)NULL, 10);
  // printf("Loading thingies... %d\n", instance);
  fd = open("/proc/usmPgs", O_WRONLY);
  if (getenv("UTHREADS")) {
    /*struct sigaction sa;                          // TODO inv. why not getting
    gud mem. zone.. and whether signal gets changed gud... meh.. sa.sa_flags =
    0; sa.sa_sigaction = (void *)sig_temporizer; sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sigaction(SIGUSR1, &sa, NULL);*/
    instance = write(fd, &instance, instance);
  } else
    instance = write(fd, NULL, instance);
  close(fd);
  if (instance) {
    printf("[Mayday] Inexisting instance, aborting.\n");
    abort();
  }
  my_uffd(O_CLOEXEC /*| O_NONBLOCK*/ | UFFD_USM);
  /*usminit: // THIS EVEN IS THE SOLUTION !      TODO : investigate.. int fdu =
     open("/proc/usmChristineSleepers", O_RDONLY); if(fdu<0) { printf("[Inf.]
     Failed to get USM com. fd!Try again?\n"); getchar(); goto usminit;
      }
      char *procCom = mmap(NULL, SYS_PAGE_SIZE, PROT_READ, MAP_SHARED, fdu, 0);
      if(procCom==MAP_FAILED) {
          perror("[Mayday] mmap failed!");
          abort();
      }
      usm_bitfield = (unsigned int
     *)(((uintptr_t)procCom)+SYS_PAGE_SIZE-sizeof(unsigned int)*2);*/
  // userfaultfd(O_CLOEXEC | O_NONBLOCK | UFFD_USM);
  // intercept_hook_point=&hook;
}
