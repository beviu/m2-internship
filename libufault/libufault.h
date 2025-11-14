#ifndef LIBUFAULT_H
#define LIBUFAULT_H

#include <x86gprintrin.h>

#ifdef BUILDING_LIBUFAULT
#define LIBUFAULT_PUBLIC __attribute__((visibility("default")))
#else
#define LIBUFAULT_PUBLIC
#endif

typedef void (*ufault_handler_t)(struct __uintr_frame *frame,
                                 unsigned long long vector);

int LIBUFAULT_PUBLIC ufault_register_handler(ufault_handler_t handler,
                                             int flags);

static inline __attribute__((always_inline)) void ufault_stuf(void) {
  asm volatile(".long 0xeb010ff3");
}

static inline __attribute__((always_inline)) void ufault_cluf(void) {
  asm volatile(".long 0xea010ff3");
}

static inline __attribute__((always_inline)) void ufault_kret(void) {
  asm volatile(".long 0xe9010ff3");
}

#endif /* LIBUFAULT_H */
