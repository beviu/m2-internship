#include <fcntl.h>
#include <inttypes.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

static uint64_t rdtsc() {
  uint32_t eax, edx;

#ifndef RDTSC_UNSERIALIZED
  asm volatile("rdtsc" : "=a"(eax), "=d"(edx));
#else
  asm volatile("mfence; lfence\n"
               "rdtsc\n"
               "lfence"
               : "=a"(eax), "=d"(edx));
#endif

  return eax | ((uint64_t)edx << 32);
}

static int userfaultfd(int flags) { return syscall(SYS_userfaultfd, flags); }

int main() {
  int ret = EXIT_SUCCESS;
  ssize_t page_size;
  void *page = MAP_FAILED;
  int fd = -1;
  struct uffdio_api api_args = {0};
  struct uffdio_register register_args = {0};
  struct uffdio_zeropage zeropage_args = {0};
  uint64_t start_tsc, end_tsc;

  page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    perror("sysconf(_SC_PAGESIZE)");
    ret = EXIT_FAILURE;
    goto out;
  }

  page = mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    ret = EXIT_FAILURE;
    goto out;
  }

  fd = userfaultfd(O_CLOEXEC);
  if (fd == -1) {
    perror("userfaultfd");
    ret = EXIT_FAILURE;
    goto out;
  }

  api_args.api = UFFD_API;
  if (ioctl(fd, UFFDIO_API, &api_args) == -1) {
    perror("ioctl(UFFDIO_API)");
    ret = EXIT_FAILURE;
    goto out;
  }

  register_args.range.start = (uint64_t)page;
  register_args.range.len = page_size;
  register_args.mode = UFFDIO_REGISTER_MODE_MISSING;
  if (ioctl(fd, UFFDIO_REGISTER, &register_args) == -1) {
    perror("ioctl(UFFDIO_REGISTER)");
    ret = EXIT_FAILURE;
    goto out;
  }

  zeropage_args.range = register_args.range;

  puts("end");

  for (int i = 0; i < 100000; ++i) {
    start_tsc = rdtsc();

    if (ioctl(fd, UFFDIO_ZEROPAGE, &zeropage_args) == -1) {
      perror("ioctl(UFFDIO_ZEROPAGE)");
      ret = EXIT_FAILURE;
      goto out;
    }

    end_tsc = rdtsc();

    printf("%" PRIu64 "\n", end_tsc - start_tsc);

    if (madvise(page, page_size, MADV_DONTNEED) == -1) {
      perror("mmap");
      ret = EXIT_FAILURE;
      goto out;
    }
  }

out:
  if (fd == -1)
    close(fd);
  if (page)
    munmap(page, page_size);

  return ret;
}
