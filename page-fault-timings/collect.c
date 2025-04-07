#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <x86intrin.h>

#ifdef USERFAULTFD
#include <stdatomic.h>
#endif

static bool parse_uint64_t(const char **buf, const char *end, uint64_t *out) {
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

static int timestamps_dir_fd = -1;

static bool read_timestamp(const char *name, uint64_t *timestamp) {
  int timestamp_fd;
  char buf[21];
  ssize_t n;
  const char *tmp;
  bool ok = false;

  timestamp_fd = openat(timestamps_dir_fd, name, O_RDONLY);
  if (timestamp_fd == -1) {
    fprintf(stderr, "open(/proc/sys/debug/fast-tracepoints/%s): %s\n", name,
            strerror(errno));
    goto fail;
  }

  n = read(timestamp_fd, buf, sizeof(buf));
  if (n == -1) {
    fprintf(stderr, "read(/proc/sys/debug/fast-tracepoints/%s): %s\n", name,
            strerror(errno));
    goto fail;
  }

  tmp = buf;
  if (!parse_uint64_t(&tmp, buf + n, timestamp)) {
    fprintf(stderr, "Failed to parse timestamp: %.*s.\n", (int)n, buf);
    goto fail;
  }

  ok = true;

fail:
  close(timestamp_fd);

  return ok;
}

#ifdef USERFAULTFD
static int userfaultfd(int flags) { return syscall(SYS_userfaultfd, flags); }

struct userfaultfd_thread_args {
  int userfaultfd_fd;
  struct uffdio_range range;
};

static _Atomic uint64_t timestamp_msg_received;

static void *userfaultfd_thread_routine(void *data) {
  size_t n;
  struct userfaultfd_thread_args *args = (struct userfaultfd_thread_args *)data;
  struct uffd_msg msg;
  unsigned int aux;
  struct uffdio_zeropage zeropage_args = {0};

  for (;;) {
    n = read(args->userfaultfd_fd, &msg, sizeof(msg));
    if (n == -1) {
      perror("read");
      break;
    } else if (n != sizeof(msg)) {
      fprintf(stderr, "Could only read %zd bytes from userfaultfd.\n", n);
      break;
    }

    atomic_store_explicit(&timestamp_msg_received, __rdtscp(&aux),
                          memory_order_relaxed);

    zeropage_args.range = args->range;
    zeropage_args.mode = 0;
    if (ioctl(args->userfaultfd_fd, UFFDIO_ZEROPAGE, &zeropage_args) == -1) {
      perror("ioctl(UFFDIO_ZEROPAGE)");
      break;
    }
  }

  return NULL;
}
#endif

int main() {
  int ret = EXIT_FAILURE;
  cpu_set_t cpu_set;
  ssize_t page_size;
  void *page = MAP_FAILED;
  int err;
  unsigned int aux;
  int i;
  uint64_t timestamp_page_fault;
  uint8_t zero;
  uint32_t timestamp_iret_1;
  uint32_t timestamp_iret_2;
  uint32_t timestamp_isr_entry_1;
  uint32_t timestamp_isr_entry_2;
  uint64_t timestamp_end;
  uint64_t timestamp_iret;
  uint64_t timestamp_isr_entry;
  uint64_t timestamp_lock_vma_under_rcu_start;
  uint64_t timestamp_lock_vma_under_rcu_end;
  uint64_t timestamp_first_handle_mm_fault_start;
  uint64_t timestamp_first_page_table_walk_end;
  uint64_t timestamp_first_handle_mm_fault_end;
#ifdef USERFAULTFD
  int userfaultfd_fd = -1;
  struct uffdio_api api_args = {0};
  struct uffdio_register register_args = {0};
  struct userfaultfd_thread_args thread_args;
  pthread_t userfaultfd_thread;
  bool started_userfaultfd_thread = false;
  uint64_t timestamp_msg_received_copy;
  uint64_t timestamp_wake_up_userfaultfd_start;
  uint64_t timestamp_wake_up_userfaultfd_end;
  uint64_t timestamp_lock_mm_and_find_vma_start;
  uint64_t timestamp_lock_mm_and_find_vma_end;
  uint64_t timestamp_second_handle_mm_fault_start;
  uint64_t timestamp_second_page_table_walk_end;
  uint64_t timestamp_second_handle_mm_fault_end;
#endif

  timestamps_dir_fd =
      open("/proc/sys/debug/fast-tracepoints", O_DIRECTORY | O_PATH);
  if (timestamps_dir_fd == -1) {
    perror("open(/proc/sys/debug/fast-tracepoints)");
    goto fail;
  }

  CPU_ZERO(&cpu_set);
  CPU_SET(0, &cpu_set);
  err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
  if (err) {
    fprintf(stderr, "pthread_setaffinity_np: %s\n", strerror(err));
    goto fail;
  }

  page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    perror("sysconf(_SC_PAGESIZE)");
    goto fail;
  }

  page = mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    fputs("Failed to allocate a page!\n", stderr);
    goto fail;
  }

#ifdef USERFAULTFD
  userfaultfd_fd = userfaultfd(O_CLOEXEC);
  if (userfaultfd_fd == -1) {
    perror("userfaultfd");
    goto fail;
  }

  api_args.api = UFFD_API;
  if (ioctl(userfaultfd_fd, UFFDIO_API, &api_args) == -1) {
    perror("ioctl(UFFDIO_API)");
    goto fail;
  }

  register_args.range.start = (uint64_t)page;
  register_args.range.len = page_size;
  register_args.mode = UFFDIO_REGISTER_MODE_MISSING;
  if (ioctl(userfaultfd_fd, UFFDIO_REGISTER, &register_args) == -1) {
    perror("ioctl(UFFDIO_REGISTER)");
    goto fail;
  }

  thread_args.userfaultfd_fd = userfaultfd_fd;
  thread_args.range = register_args.range;
  err = pthread_create(&userfaultfd_thread, NULL, userfaultfd_thread_routine,
                       &thread_args);
  if (err) {
    fprintf(stderr, "pthread_create: %s\n", strerror(err));
    goto fail;
  }

  started_userfaultfd_thread = true;

#ifdef TWO_CPUS
  CPU_ZERO(&cpu_set);
  CPU_SET(1, &cpu_set);
#endif
  err = pthread_setaffinity_np(userfaultfd_thread, sizeof(cpu_set), &cpu_set);
  if (err) {
    fprintf(stderr, "pthread_setaffinity_np: %s\n", strerror(err));
    goto fail;
  }

  puts("isr_entry, lock_vma_under_rcu_start, lock_vma_under_rcu_end, "
       "first_handle_mm_fault_start, first_page_table_walk_end, "
       "wake_up_userfaultfd_start, wake_up_userfaultfd_end, "
       "first_handle_mm_fault_end, lock_mm_and_find_vma_start, "
       "lock_mm_and_find_vma_end, second_handle_mm_fault_start, "
       "second_page_table_walk_end, second_handle_mm_fault_end, iret, end");
#else
  puts("isr_entry, lock_vma_under_rcu_start, lock_vma_under_rcu_end, "
       "first_handle_mm_fault_start, first_page_table_walk_end, "
       "first_handle_mm_fault_end, iret, end");
#endif

  for (i = 0; i < 100000; ++i) {
    timestamp_page_fault = __rdtscp(&aux);

    /* Trigger a page fault with the magic value in ebx. */
    asm volatile("movb (%[page]), %[zero]"
                 : "=&a"(timestamp_iret_1), "=&d"(timestamp_iret_2),
                   "=&D"(timestamp_isr_entry_1),
                   "=&S"(timestamp_isr_entry_2), [zero] "=r"(zero)
                 : [page] "r"(page), "b"(0xb141a52a)
                 : "ecx");

    timestamp_end = __rdtscp(&aux);

    timestamp_iret = timestamp_iret_1 | ((uint64_t)timestamp_iret_2 << 32);
    timestamp_isr_entry =
        timestamp_isr_entry_1 | ((uint64_t)timestamp_isr_entry_2 << 32);

#define R(name) read_timestamp(#name, &timestamp_##name)

    if (!R(lock_vma_under_rcu_start) || !R(lock_vma_under_rcu_end) ||
        !R(first_handle_mm_fault_start) || !R(first_page_table_walk_end) ||
        !R(first_handle_mm_fault_end))
      goto fail;

#ifdef USERFAULTFD
    timestamp_msg_received_copy =
        atomic_load_explicit(&timestamp_msg_received, memory_order_relaxed);
    if (timestamp_msg_received_copy == 0) {
      fputs("userfaultfd thread was not notified.\n", stderr);
      ret = EXIT_FAILURE;
      goto fail;
    }

    if (!R(wake_up_userfaultfd_start) || !R(wake_up_userfaultfd_end) ||
        !R(lock_mm_and_find_vma_start) || !R(lock_mm_and_find_vma_end) ||
        !R(second_handle_mm_fault_start) || !R(second_page_table_walk_end) ||
        !R(second_handle_mm_fault_end))
      goto fail;

    printf("%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           "\n",
           timestamp_isr_entry - timestamp_page_fault,
           timestamp_lock_vma_under_rcu_start - timestamp_page_fault,
           timestamp_lock_vma_under_rcu_end - timestamp_page_fault,
           timestamp_first_handle_mm_fault_start - timestamp_page_fault,
           timestamp_first_page_table_walk_end - timestamp_page_fault,
           timestamp_wake_up_userfaultfd_start - timestamp_page_fault,
           timestamp_wake_up_userfaultfd_end - timestamp_page_fault,
           timestamp_first_handle_mm_fault_end - timestamp_page_fault,
           timestamp_lock_mm_and_find_vma_start - timestamp_page_fault,
           timestamp_lock_mm_and_find_vma_end - timestamp_page_fault,
           timestamp_second_handle_mm_fault_start - timestamp_page_fault,
           timestamp_second_page_table_walk_end - timestamp_page_fault,
           timestamp_second_handle_mm_fault_end - timestamp_page_fault,
           timestamp_iret - timestamp_page_fault,
           timestamp_end - timestamp_page_fault);

    atomic_store_explicit(&timestamp_msg_received, 0, memory_order_relaxed);
#else
    printf("%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n",
           timestamp_isr_entry - timestamp_page_fault,
           timestamp_lock_vma_under_rcu_start - timestamp_page_fault,
           timestamp_lock_vma_under_rcu_end - timestamp_page_fault,
           timestamp_first_handle_mm_fault_start - timestamp_page_fault,
           timestamp_first_page_table_walk_end - timestamp_page_fault,
           timestamp_first_handle_mm_fault_end - timestamp_page_fault,
           timestamp_iret - timestamp_page_fault,
           timestamp_end - timestamp_page_fault);
#endif

#undef R

    if (madvise(page, page_size, MADV_DONTNEED) == -1) {
      perror("mmap");
      goto fail;
    }
  }

  ret = EXIT_SUCCESS;

fail:
#ifdef USERFAULTFD
  if (started_userfaultfd_thread) {
    pthread_cancel(userfaultfd_thread);
    pthread_join(userfaultfd_thread, NULL);
  }
  if (userfaultfd_fd != -1)
    close(userfaultfd_fd);
#endif
  if (page != MAP_FAILED)
    munmap(page, page_size);
  if (timestamps_dir_fd != -1)
    close(timestamps_dir_fd);

  return ret;
}
