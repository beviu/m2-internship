#include <libufault.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static sig_atomic_t ufault_count = 0;

static void __attribute__((interrupt, target("general-regs-only")))
handler(struct __uintr_frame *frame, unsigned long long vector) {
  (void)frame;
  (void)vector;
  ++ufault_count;
}

static bool do_page_fault(void) {
  void *page;

  page = mmap(0, 1, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    return false;
  }

  *(volatile char *)page;

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

  printf("Count: %d\n", ufault_count);

  return EXIT_SUCCESS;
}
