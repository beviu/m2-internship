#ifndef LIBUFAULT_H
#define LIBUFAULT_H

#ifdef BUILDING_LIBUFAULT
#define LIBUFAULT_PUBLIC __attribute__((visibility("default")))
#else
#define LIBUFAULT_PUBLIC
#endif

int LIBUFAULT_PUBLIC lib_func();

#endif /* LIBUFAULT_H */
