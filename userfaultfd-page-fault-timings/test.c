#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

static int userfaultfd(int flags) {
  return syscall(SYS_userfaultfd, flags);
}

static void *faulting_thread_routine(void *page) {
  *(volatile char *)page;
  
  return NULL;
}

int main() {
  int fd = userfaultfd(O_CLOEXEC);
  if (fd == -1) {
    perror("userfaultfd");
    return EXIT_FAILURE;
  }

  struct uffdio_api api_args = {};
  api_args.api = UFFD_API;
  if (ioctl(fd, UFFDIO_API, &api_args) == -1) {
    perror("ioctl(UFFDIO_API)");
    close(fd);
    return EXIT_FAILURE;
  }

  ssize_t page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    perror("sysconf(_SC_PAGESIZE)");
    return EXIT_FAILURE;
  }

  void *page =
      mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    return EXIT_FAILURE;
  }

  struct uffdio_register register_args = {};
  register_args.range.start = (uint64_t)page;
  register_args.range.len = page_size;
  register_args.mode = UFFDIO_REGISTER_MODE_MISSING;
  if (ioctl(fd, UFFDIO_REGISTER, &register_args) == -1) {
    perror("ioctl(UFFDIO_REGISTER)");
    close(fd);
    return EXIT_FAILURE;
  }

  pthread_t faulting_thread;
  int ret = pthread_create(&faulting_thread, NULL, faulting_thread_routine, page);
  if (ret) {
    fprintf(stderr, "pthread_create: %s\n", strerror(ret));
    close(fd);
    return EXIT_FAILURE;
  }

  struct uffd_msg msg;
  ssize_t n = read(fd, &msg, sizeof(msg));
  if (n != sizeof(msg)) {
    if (n == -1) {
      perror("read");
    } else {
      fprintf(stderr, "Could only read %zd bytes from userfaultfd.\n", n);
    }
    pthread_kill(faulting_thread, SIGTERM);
    pthread_join(faulting_thread, NULL);
    close(fd);
    return EXIT_FAILURE;
  }

  struct uffdio_zeropage zeropage_args;
  zeropage_args.range = register_args.range;
  zeropage_args.mode = 0;
  if (ioctl(fd, UFFDIO_ZEROPAGE, &zeropage_args) == -1) {
    perror("ioctl(UFFDIO_ZEROPAGE)");
    pthread_kill(faulting_thread, SIGTERM);
    pthread_join(faulting_thread, NULL);
    close(fd);
    return EXIT_FAILURE;  
  }

  pthread_join(faulting_thread, NULL);
  close(fd);
  
  return EXIT_SUCCESS;
}
