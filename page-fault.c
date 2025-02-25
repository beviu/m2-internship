#include <inttypes.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <x86intrin.h>

int main() {
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

  printf("time\n");

  for (int i = 0; i < 100000; ++i) {
    int64_t start_counter = __rdtsc();

    *(volatile char *)page;

    int64_t end_counter = __rdtsc();

    printf("%" PRIi64 "\n", end_counter - start_counter);

    if (madvise(page, page_size, MADV_DONTNEED) == -1) {
      perror("mmap");
      munmap(page, page_size);
      return EXIT_FAILURE;
    }
  }

  munmap(page, page_size);

  return 0;
}
