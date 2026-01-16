#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <x86gprintrin.h>

#define cluf() asm volatile(".long 0xea010ff3" ::: "memory");
#define stuf() asm volatile(".long 0xeb010ff3" ::: "memory");

typedef void (*ufault_handler_t)(struct __uintr_frame *frame,
                                 unsigned long long vector);

static sig_atomic_t ufault_code;
static sig_atomic_t ufault_count = 0;

static inline __attribute__((always_inline)) bool testuf(void) {
  bool uff;

  asm volatile(".long 0xeb010ff3" : "=@ccc"(uff)::"memory");

  return uff;
}

static long syscall_2(long num, long arg1, long arg2) {
  long ret;
  asm volatile("syscall"
               : "=a"(ret)
               : "0"(num), "D"(arg1), "S"(arg2)
               : "rcx", "r11", "memory", "cc");
  return ret;
}

static int ufault_register_handler(ufault_handler_t handler, int flags) {
  return syscall_2(480, (long)handler, flags);
}

static void __attribute__((interrupt, target("uintr,general-regs-only")))
handler(struct __uintr_frame *frame, unsigned long long params) {
  uint8_t code = params >> 60;
  void *addr = (void *)((params << 3) >> 3);

  // Let the kernel handle the fault.
  cluf();
  *(volatile char *)addr;
  stuf();

  ufault_code = params >> 60;
  ++ufault_count;
}

static bool test_uff(void) {
  bool uff;

  if (testuf()) {
    fputs("UFF flag is set before running STUF.\n", stderr);
    return false;
  }

  stuf();
  uff = testuf();
  cluf();

  if (!uff) {
    fputs("UFF flag is not set after running STUF.\n", stderr);
    return false;
  }

  return true;
}

static bool test_user_fault(void) {
  int ret;
  void *page = NULL;
  bool ok = true;

  ret = ufault_register_handler(handler, 0);
  if (ret) {
    fprintf(stderr, "ufault_register_handler: %s\n", strerror(-ret));
    ok = false;
    goto out;
  }

  page = mmap(0, 1, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    ok = false;
    goto out;
  }

  stuf();
  *(volatile char *)page;
  cluf();

  if (ufault_count != 1) {
    fprintf(stderr, "Unexpected User Fault count: %d\n", ufault_count);
    ok = false;
    goto out;
  }

  if (ufault_code != 0) {
    fprintf(stderr, "Unexpected User Fault code: %d\n", ufault_code);
    ok = false;
    goto out;
  }

out:
  if (page)
    munmap(page, 1);

  return ok;
}

int main(void) {
  int ret;

  if (!test_uff() || !test_user_fault())
    return EXIT_FAILURE;

  puts("User Faults are working.");

  return EXIT_SUCCESS;
}
