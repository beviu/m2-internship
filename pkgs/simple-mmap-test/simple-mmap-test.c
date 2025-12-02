#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int main() {
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    perror("sysconf(_SC_PAGESIZE)");
    return EXIT_FAILURE;
  }

  puts("Mapping memory...");

  const size_t len = 2UL * 1024UL * 1024UL * 1024UL;

  void *addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED) {
    perror("mmap");
    return EXIT_FAILURE;
  }

  puts("Writing to memory map...");

  int last_percentage = -1;

  const size_t total = 1024UL * 1024UL;
  for (size_t i = 0; i < total; i += page_size) {
    ((volatile char *)addr)[i] = 1;

    int percentage = i * 100UL / total;
    if (percentage > last_percentage) {
      printf("\rProgress: %d%%", percentage);
      fflush(stdout);
      last_percentage = percentage;
    }
  }

  puts("\rProgress: 100%");

  munmap(addr, len);

  return EXIT_SUCCESS;
}
