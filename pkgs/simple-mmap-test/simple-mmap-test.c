#include <gem5/m5ops.h>
#include <m5_mmap.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void write_fault_pages(void *addr, size_t len, size_t page_size) {
  int last_percentage = -1;

  for (size_t i = 0; i < len; i += page_size) {
    ((volatile char *)addr)[i] = 1;

    int percentage = i * 100UL / len;
    if (percentage > last_percentage) {
      printf("\rProgress: %d%%", percentage);
      fflush(stdout);
      last_percentage = percentage;
    }
  }

  puts("\rProgress: 100%");
}

static bool env_var_is_true(const char *name) {
  const char *value = getenv(name);
  return value && strcmp(value, "1") == 0;
}

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

  if (env_var_is_true("SIMPLE_MMAP_TEST_M5_EXIT_BEFORE")) {
    map_m5_mem();
    m5_exit_addr(0);
  }

  write_fault_pages(addr, 1024UL * 1024UL, page_size);

  if (env_var_is_true("SIMPLE_MMAP_TEST_M5_EXIT_AFTER")) {
    map_m5_mem();
    m5_exit_addr(0);
  }

  munmap(addr, len);

  return EXIT_SUCCESS;
}
