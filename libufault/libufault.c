#include <libufault.h>

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

int ufault_register_handler(ufault_handler_t handler, int flags) {
  return syscall_2(480, (long)handler, flags);
}

int ufault_unregister_handler(int flags) {
  return syscall_1(481, flags);
}
