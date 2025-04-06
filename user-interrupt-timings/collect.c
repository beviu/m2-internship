#include <stdint.h>
#include <stdlib.h>

static long syscall_1(long num, long arg1) {
  long ret;
  asm volatile("syscall"
               : "=a"(ret)
               : "0"(num), "D"(arg1)
               : "rcx", "r11", "memory", "cc");
  return ret;
}

static long syscall_2(long num, long arg1, long arg2) {
  long ret;
  asm volatile("syscall"
               : "=a"(ret)
               : "0"(num), "D"(arg1), "S"(arg2)
               : "rcx", "r11", "memory", "cc");
  return ret;
}

static int uintr_register_handler(uint64_t vector, unsigned int flags) {
  return syscall_2(473, vector, flags);
}

static int uintr_vector_fd(uint64_t vector, unsigned int flags) {
  return syscall_2(473, vector, flags);
}

int main() { return EXIT_SUCCESS; }
