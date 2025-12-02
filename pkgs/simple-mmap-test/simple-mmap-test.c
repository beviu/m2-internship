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

  const size_t len = 2L * 1024L * 1024L * 1024L;
  void *addr = mmap(NULL, len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED) {
    perror("mmap");
    return EXIT_FAILURE;
  }

  for (size_t i = 0; i < 1024L * 1024L; i += len)
    ((volatile char *)addr)[i] = 1;

  munmap(addr, len);

  return EXIT_SUCCESS;
}
