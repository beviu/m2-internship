#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <unistd.h>
#include <x86gprintrin.h>

static long syscall_1(long num, long arg1) {
  long ret;

  asm volatile("syscall"
               : "=a"(ret)
               : "0"(num), "D"(arg1)
               : "rcx", "r11", "memory", "cc");

  return ret;
}

static long syscall_2(long num, long arg1, long arg2) {
  long ret;

  asm volatile("syscall"
               : "=a"(ret)
               : "0"(num), "D"(arg1), "S"(arg2)
               : "rcx", "r11", "memory", "cc");

  return ret;
}

typedef void (*uintr_handler_t)(struct __uintr_frame *frame,
                                unsigned long long vector);

static int uintr_register_handler(uintr_handler_t handler, unsigned int flags) {
  return syscall_2(471, (long)handler, flags);
}

static int uintr_unregister_handler(unsigned int flags) {
  return syscall_1(472, flags);
}

static void __attribute__((interrupt)) handler(struct __uintr_frame *frame,
                                               unsigned long long vector) {
  /* No need to preserve registers as _Exit does not return. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wexcessive-regsave"
  _Exit(EXIT_SUCCESS);
#pragma GCC diagnostic pop
}

static void page_fault() {
  long page_size;
  void *page;

  page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    perror("sysconf(_SC_PAGESIZE)");
    return;
  }

  page = mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    return;
  }

  *(volatile char *)page;

  munmap(page, page_size);
}

int main(void) {
  int ret;

  ret = uintr_register_handler(handler, 0);
  if (ret < 0) {
    fprintf(stderr, "uintr_register_handler: %s\n", strerror(-ret));
    return EXIT_FAILURE;
  }

  page_fault();

  ret = uintr_unregister_handler(0);
  if (ret < 0)
    fprintf(stderr, "uintr_unregister_handler: %s\n", strerror(-ret));

  return EXIT_FAILURE;
}
