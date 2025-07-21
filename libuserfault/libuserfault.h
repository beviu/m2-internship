#ifndef LIBUSERFAULT_H
#define LIBUSERFAULT_H

#ifdef BUILDING_LIBUSERFAULT
#define LIBUSERFAULT_PUBLIC __attribute__((visibility("default")))
#else
#define LIBUSERFAULT_PUBLIC
#endif

int LIBUSERFAULT_PUBLIC lib_func();

#endif /* LIBUSERFAULT_H */
