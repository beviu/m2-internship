#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <x86intrin.h>

ssize_t page_size;
void *page;
int64_t signal_counter;
int64_t return_counter;

void handle_sigsegv(int signal_number) {
  (void)signal_number;
  signal_counter = __rdtsc();
  mprotect(page, page_size, PROT_READ);
  return_counter = __rdtsc();
}

int main() {
  page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    perror("sysconf(_SC_PAGESIZE)");
    return EXIT_FAILURE;
  }

  page = mmap(NULL, page_size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    return EXIT_FAILURE;
  }

  signal(SIGSEGV, handle_sigsegv);

  printf("call_time, mprotect_time, return_time\n");

  for (int i = 0; i < 100000; ++i) {
    int64_t start_counter = __rdtsc();

    /* Trigger SIGSEGV. */
    *(volatile char *)page;

    int64_t end_counter = __rdtsc();

    printf("%" PRIi64 ", %" PRIi64 ", %" PRIi64 "\n",
           signal_counter - start_counter, return_counter - start_counter,
           end_counter - return_counter);

    mprotect(page, page_size, PROT_NONE);
  }

  munmap(page, page_size);

  return 0;
}
