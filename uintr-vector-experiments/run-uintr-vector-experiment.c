#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>
#include <x86gprintrin.h>

#define UINTR_HANDLER_FLAG_SET_UINV 0x1
#define UINTR_HANDLER_FLAG_UINV_SHIFT 24
#define UINTR_HANDLER_FLAG_UINV_MASK 0xff000000
#define UINTR_HANDLER_FLAG_SET_UPID_NV 0x2
#define UINTR_HANDLER_FLAG_UPID_NV_SHIFT 16
#define UINTR_HANDLER_FLAG_UPID_NV_MASK 0x00ff0000
#define UINTR_HANDLER_FLAG_SET_UPIR 0x4
#define UINTR_HANDLER_FLAG_NO_SELF_IPI 0x8

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

typedef void (*uintr_handler_t)(struct __uintr_frame *frame,
                                unsigned long long vector);

static int uintr_register_handler(uintr_handler_t handler, unsigned int flags) {
  return syscall_2(471, (long)handler, flags);
}

static int uintr_unregister_handler(unsigned int flags) {
  return syscall_1(472, flags);
}

static int uintr_register_self(uint8_t vector, unsigned int flags) {
  return syscall_2(477, vector, flags);
}

static sig_atomic_t uintr_count = 0;

static void __attribute__((interrupt, target("general-regs-only")))
uintr_handler(struct __uintr_frame *frame, unsigned long long vector) {
  (void)frame;
  (void)vector;
  ++uintr_count;
}

static bool parse_uint32_t(const char **buf, const char *end, uint32_t *out) {
  if (*buf == end || **buf < '0' || **buf > '9')
    return false;

  *out = 0;

  while (*buf != end && **buf >= '0' && **buf <= '9') {
    *out *= 10;
    *out += **buf - '0';
    ++*buf;
  }

  return true;
}

static sig_atomic_t received_sigint = 0;

static void sigint_handler(int signal) {
  (void)signal;
  received_sigint = 1;
}

static const struct option long_options[] = {
    {"uinv", required_argument, 0, 'u'},
    {"upid-nv", required_argument, 0, 'n'},
    {"set-upir", no_argument, 0, 'U'},
    {"no-self-ipi", no_argument, 0, 'I'},
    {"senduipi", no_argument, 0, 's'},
    {"page-fault", no_argument, 0, 'p'},
    {"spin", optional_argument, 0, 'S'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}};

static void print_usage(const char *arg0) {
  printf(
      "Usage: %s [OPTION]...\n"
      "Run a user interrupt vector experiment.\n"
      "\n"
      "Mandatory arguments to long options are mandatory for short options "
      "too.\n"
      "  -u, --uinv=VECTOR     set the notification vector in the "
      "MSR_IA32_UINTR_MISC MSR (UINV) to VECTOR\n"
      "  -n, --upid-nv=VECTOR  set the NV field in the UPID to VECTOR\n"
      "  -U, --set-upir        set the UPIR to a nonzero value\n"
      "  -I, --no-self-ipi     disable delivery of outstanding notifications "
      "using self IPIs on user mode return\n"
      "  -s, --senduipi        do a SENDUIPI\n"
      "  -p, --page-fault      do a page fault after registering the UPID\n"
      "  -S, --spin=SECONDS    spin for SECONDS seconds before unregistering "
      "the handler\n"
      "  -h, --help            display this help message and exit\n",
      arg0);
}

int main(int argc, char **argv) {
  const char *arg0;
  int ret;
  int opt;
  const char *cursor;
  uint32_t irq_vector = 0;
  unsigned int flags = 0;
  bool do_senduipi = false;
  bool do_page_fault = false;
  uint32_t spin = 0;
  ssize_t page_size;
  void *page;
  struct sigaction sigint_action;
  int err;
  int uipi_index;
  struct timespec spin_stop_time;
  struct timespec now;

  arg0 = argv[0] ? argv[0] : "run-uintr-vector-experiment";

  while ((opt = getopt_long(argc, argv, "u:n:UIs::pSh", long_options, NULL)) !=
         -1) {
    switch (opt) {
    case 'u':
    case 'n':
      cursor = optarg;
      if (!parse_uint32_t(&cursor, optarg + strlen(optarg), &irq_vector) ||
          *cursor || irq_vector > UINT8_MAX) {
        fprintf(stderr, "%s: invalid IRQ vector number: ‘%s’\n", arg0, optarg);
        ret = EXIT_FAILURE;
        goto out;
      }
      flags |= opt == 'u' ? UINTR_HANDLER_FLAG_SET_UINV
                          : UINTR_HANDLER_FLAG_SET_UPID_NV;
      flags &= ~(opt == 'u' ? UINTR_HANDLER_FLAG_UINV_MASK
                            : UINTR_HANDLER_FLAG_UPID_NV_MASK);
      flags |= irq_vector << (opt == 'u' ? UINTR_HANDLER_FLAG_UINV_SHIFT
                                         : UINTR_HANDLER_FLAG_UPID_NV_SHIFT);
      break;
    case 'U':
      flags |= UINTR_HANDLER_FLAG_SET_UPIR;
      break;
    case 'I':
      flags |= UINTR_HANDLER_FLAG_NO_SELF_IPI;
      break;
    case 's':
      do_senduipi = true;
      break;
    case 'p':
      do_page_fault = true;
      break;
    case 'S':
      if (optarg) {
        cursor = optarg;
        if (!parse_uint32_t(&cursor, optarg + strlen(optarg), &spin) ||
            *cursor) {
          fprintf(stderr, "%s: invalid number of seconds: ‘%s’\n", arg0,
                  optarg);
          ret = EXIT_FAILURE;
          goto out;
        }
      } else {
        spin = UINT32_MAX;
      }
      break;
    case 'h':
      print_usage(arg0);
      ret = EXIT_SUCCESS;
      goto out;
    default:
      fprintf(stderr, "Try '%s --help' for more information.\n", arg0);
      ret = EXIT_SUCCESS;
      goto out;
    }
  }

  if (do_page_fault) {
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
      perror("sysconf(_SC_PAGESIZE)");
      ret = EXIT_FAILURE;
      goto out;
    }

    page = mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (page == MAP_FAILED) {
      perror("mmap");
      ret = EXIT_FAILURE;
      goto out;
    }
  }

  if (spin) {
    sigint_action.sa_handler = sigint_handler;
    sigemptyset(&sigint_action.sa_mask);
    sigint_action.sa_flags = 0;

    if (sigaction(SIGINT, &sigint_action, NULL) == -1) {
      perror("sigaction");
      ret = EXIT_FAILURE;
      goto cleanup_page;
    }
  }

  _stui();

  err = uintr_register_handler(uintr_handler, flags);
  if (err < 0) {
    fprintf(stderr, "uintr_register_handler: %s\n", strerror(-err));
    ret = EXIT_FAILURE;
    goto cleanup_page;
  }

  if (do_senduipi) {
    uipi_index = uintr_register_self(0, 0);
    if (uipi_index < 0) {
      fprintf(stderr, "uintr_register_self: %s\n", strerror(-uipi_index));
      ret = EXIT_FAILURE;
      goto cleanup;
    }

    _senduipi(uipi_index);
  }

  if (do_page_fault)
    *(volatile char *)page;

  if (spin) {
    if (clock_gettime(CLOCK_MONOTONIC, &spin_stop_time) == -1) {
      perror("clock_gettime");
      ret = EXIT_FAILURE;
      goto cleanup;
    }

    spin_stop_time.tv_sec += spin;

    while (!received_sigint) {
      if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        perror("clock_gettime");
        ret = EXIT_FAILURE;
        goto cleanup;
      }
      if (now.tv_sec > spin_stop_time.tv_sec ||
          (now.tv_sec == spin_stop_time.tv_sec &&
           now.tv_nsec >= spin_stop_time.tv_nsec))
        break;
    }
  }

  _clui();

  printf("Received %d interrupt(s).\n", uintr_count);

  ret = EXIT_SUCCESS;

cleanup:
  err = uintr_unregister_handler(0);
  if (err < 0)
    fprintf(stderr, "uintr_unregister_handler: %s\n", strerror(-err));

cleanup_page:
  if (do_page_fault)
    munmap(page, page_size);

out:
  return ret;
}
