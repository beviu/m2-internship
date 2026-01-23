#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <x86gprintrin.h>

#define PR_UFAULT 79
#define PR_UFAULT_DISABLE 0
#define PR_UFAULT_ENABLE 1
#define PR_UFAULT_STATUS 2

#define UFAULT_CODE_SHIFT 60
#define UFAULT_CODE_PRESENT (1 << 0)
#define UFAULT_CODE_WRITE (1 << 2)
#define UFAULT_CODE_FETCH (1 << 3)
#define UFAULT_ADDR_MASK ((1ULL << UFAULT_CODE_SHIFT) - 1)

#define cluf() asm volatile(".long 0xea010ff3" ::: "memory");
#define stuf() asm volatile(".long 0xeb010ff3" ::: "memory");

static sig_atomic_t ufault_code;
static sig_atomic_t ufault_count = 0;

static int prctl_ufault(unsigned long opt, unsigned long arg) {
  return prctl((unsigned long)PR_UFAULT, opt, arg, 0L, 0L);
}

static inline __attribute__((always_inline)) bool testuf(void) {
  bool uff;
  asm(".long 0xe9010ff3" : "=@ccc"(uff)::"cc");
  return uff;
}

static void __attribute__((interrupt, target("uintr,general-regs-only")))
ufault_handler(struct __uintr_frame *frame, unsigned long long params) {
  uint8_t code = params >> UFAULT_CODE_SHIFT;
  void *addr = (void *)(params & UFAULT_ADDR_MASK);

  // Let the kernel handle the fault.
  cluf();
  if (code & UFAULT_CODE_WRITE)
    *(volatile char *)addr = 0;
  else
    *(volatile char *)addr;
  stuf();

  if (ufault_count++)
    ufault_code = params >> 60;
}

static bool test_initial_uff(void) {
  if (testuf()) {
    fputs("UFF should be unset at the start of the process.\n", stderr);
    return false;
  }

  return true;
}

static bool test_stuf(void) {
  stuf();

  if (!testuf()) {
    fputs("UFF should be set after calling STUF.\n", stderr);
    return false;
  }

  return true;
}

static bool test_cluf(void) {
  cluf();

  if (testuf()) {
    fputs("UFF should be unset after calling CLUF.\n", stderr);
    return false;
  }

  return true;
}

static bool test_initial_prctl_ufault_status(void) {
  int status = prctl_ufault(PR_UFAULT_STATUS, 0);
  if (status == -1) {
    perror("prctl(PR_UFAULT, PR_UFAULT_STATUS)");
    return false;
  }

  if (status) {
    fputs("PR_UFAULT_STATUS should return 0 at the start of the process.\n",
          stderr);
    return false;
  }

  return true;
}

static bool test_prctl_ufault_enable(void) {
  int status;

  if (prctl_ufault(PR_UFAULT_ENABLE, (unsigned long)NULL) == -1) {
    perror("prctl(PR_UFAULT, PR_UFAULT_ENABLE)");
    return false;
  }

  status = prctl_ufault(PR_UFAULT_STATUS, 0);
  if (status == -1) {
    perror("prctl(PR_UFAULT, PR_UFAULT_STATUS)");
    return false;
  }

  if (!status) {
    fputs("PR_UFAULT_STATUS should return 1 after calling PR_UFAULT_ENABLE.\n",
          stderr);
    return false;
  }

  return true;
}

static bool test_prctl_ufault_disable(void) {
  int status;

  if (prctl_ufault(PR_UFAULT_DISABLE, (unsigned long)NULL) == -1) {
    perror("prctl(PR_UFAULT, PR_UFAULT_DISABLE)");
    return false;
  }

  status = prctl_ufault(PR_UFAULT_STATUS, 0);
  if (status == -1) {
    perror("prctl(PR_UFAULT, PR_UFAULT_STATUS)");
    return false;
  }

  if (status) {
    fputs("PR_UFAULT_STATUS should return 0 after calling PR_UFAULT_DISABLE.\n",
          stderr);
    return false;
  }

  return true;
}

static bool test_delivery(void) {
  void *page = NULL;

  if (prctl_ufault(PR_UFAULT_ENABLE, (unsigned long)ufault_handler) == -1) {
    perror("prctl(PR_UFAULT, PR_UFAULT_ENABLE)");
    return false;
  }

  page = mmap(0, 1, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    return false;
  }

  stuf();
  *(volatile char *)page;
  cluf();

  if (ufault_count != 1) {
    fprintf(stderr, "A read should trigger one user fault, not %d.\n",
            ufault_count);
    munmap(page, 1);
    return false;
  }

  if (ufault_code != 0) {
    fprintf(stderr, "A read should have code 0, not %d.\n", ufault_code);
    munmap(page, 1);
    return false;
  }

  munmap(page, 1);

  return true;
}

int main(void) {
  bool ok = true;

  ok &= test_initial_uff();
  ok &= test_stuf();
  ok &= test_cluf();
  ok &= test_initial_prctl_ufault_status();
  ok &= test_prctl_ufault_enable();
  ok &= test_prctl_ufault_disable();
  ok &= test_delivery();

  if (ok) {
    puts("User Faults are working!");
    return EXIT_SUCCESS;
  }

  return EXIT_FAILURE;
}
