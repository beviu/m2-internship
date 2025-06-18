#pragma once
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <x86intrin.h>
extern int shm[7];
extern volatile unsigned long long term[7];
#ifdef UINTR
#include <x86gprintrin.h>

// System call numbers for UINTR related functions
#define __NR_uintr_register_handler 471
#define __NR_uintr_vector_fd 473
#define __NR_uintr_register_sender 474
#define __NR_uintr_wait 476

// Macros to call system calls for UINTR functionality
#define uintr_register_handler(handler, flags)                                 \
  syscall(__NR_uintr_register_handler, handler, flags)
#define uintr_create_fd(vector, flags)                                         \
  syscall(__NR_uintr_vector_fd, vector, flags)
#define uintr_register_sender(fd, flags)                                       \
  syscall(__NR_uintr_register_sender, fd, flags)
// TODO rid kernel side of 0 without effect..
#define uintr_wait() syscall(__NR_uintr_wait, 10 /*0*/, 0)
#endif
#ifdef TSERV
static inline void rdtsc(uint64_t *time) {
  __asm__ __volatile__("mfence\n"
                       "lfence\n");
  *time = _rdtsc();
  __asm__ __volatile__("lfence\n");
}
#endif

// Macro to check the result of a function and exit on failure
#define CHECK_FUNC(func_call)                                                  \
  do {                                                                         \
    if (!(func_call)) {                                                        \
      fprintf(stderr, "%s:%d: %s() failed\n", __FILE__, __LINE__, #func_call); \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)
