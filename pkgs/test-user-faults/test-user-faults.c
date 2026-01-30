#include <inttypes.h>
#include <stdbool.h>
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

#define UFAULT_CODE_PRESENT (1 << 0)
#define UFAULT_CODE_WRITE (1 << 1)
#define UFAULT_CODE_FETCH (1 << 2)

volatile uint32_t ufault_count = 0;
volatile uint64_t first_ufault_code;

static inline __attribute__((always_inline)) bool testuf(void) {
  bool uff;
  asm(".long 0xe9010ff3" : "=@ccc"(uff)::"cc");
  return uff;
}

static inline __attribute__((always_inline)) void cluf(void) {
  asm volatile(".long 0xea010ff3" :::);
}

static inline __attribute__((always_inline)) void stuf(void) {
  asm volatile(".long 0xeb010ff3" :::);
}

static inline __attribute__((always_inline)) void barrier(void) {
  asm volatile("" ::: "memory");
}

static int prctl_ufault(unsigned long opt, unsigned long arg) {
  return prctl((unsigned long)PR_UFAULT, opt, arg, 0L, 0L);
}

// see test-user-faults.S
extern void asm_ufault_handler();

__attribute__((target("general-regs-only")))
void ufault_handler(struct __uintr_frame *frame, unsigned long long addr,
                    unsigned long long code) {
  /* Replay the fault with user faults disabled to let the kernel handle it. */
  if (code & UFAULT_CODE_WRITE)
    *(volatile char *)addr = 0;
  else
    *(volatile char *)addr;

  if (ufault_count++)
    first_ufault_code = code;
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

  if (prctl_ufault(PR_UFAULT_ENABLE, (unsigned long)asm_ufault_handler) == -1) {
    perror("prctl(PR_UFAULT, PR_UFAULT_ENABLE)");
    return false;
  }

  page = mmap(0, 1, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    return false;
  }

  barrier();
  stuf();
  barrier();

  *(volatile char *)page;

  barrier();
  cluf();
  barrier();

  if (ufault_count != 1) {
    fprintf(stderr, "A read should trigger one user fault, not %d.\n",
            ufault_count);
    munmap(page, 1);
    return false;
  }

  if (first_ufault_code != 0) {
    fprintf(stderr, "A read should have code 0, not %" PRIu64 ".\n",
            first_ufault_code);
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
