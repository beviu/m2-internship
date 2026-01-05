#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <x86gprintrin.h>

typedef void (*ufault_handler_t)(struct __uintr_frame *frame,
                                 unsigned long long vector);

static sig_atomic_t ufault_count = 0;

static inline __attribute__((always_inline)) void ufault_stuf(void) {
  asm volatile(".long 0xeb010ff3" :::);
}

static inline __attribute__((always_inline)) void ufault_cluf(void) {
  asm volatile(".long 0xea010ff3" :::);
}

static inline __attribute__((always_inline, no_caller_saved_registers,
                             target("general-regs-only"))) void
ufault_kret(void) {
  asm volatile(".long 0xe9010ff3" ::: "memory");
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
handler(struct __uintr_frame *frame, unsigned long long vector) {
  (void)frame;
  (void)vector;

  ++ufault_count;

  ufault_kret();
}

static bool do_page_fault(void) {
  void *page;

  page = mmap(0, 1, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    return false;
  }

  ufault_stuf();

  *(volatile char *)page;

  ufault_cluf();

  munmap(page, 1);

  return true;
}

int main(void) {
  int ret;

  ret = ufault_register_handler(handler, 0);
  if (ret) {
    fprintf(stderr, "ufault_register_handler: %s\n", strerror(-ret));
    return EXIT_FAILURE;
  }

  if (!do_page_fault())
    return EXIT_FAILURE;

  if (ufault_count != 1) {
    fprintf(stderr, "Unexpected User Fault count: %d\n", ufault_count);
    return EXIT_FAILURE;
  }

  puts("User Faults are working.");

  return EXIT_SUCCESS;
}
