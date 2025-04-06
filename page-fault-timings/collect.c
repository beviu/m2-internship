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

static bool read_timestamp(int dir_fd, const char *name, uint64_t *timestamp) {
  int fd;
  char buf[20];
  ssize_t n;
  
  fd = openat(dir_fd, name, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "open(/proc/sys/debug/fast-tracepoints/%s): %s\n", name, strerror(errno));
    return false;
  }

  n = read(fd, buf, sizeof(buf));
  if (n == -1) {
    fprintf(stderr, "read(/proc/sys/debug/fast-tracepoints/%s): %s\n", name, strerror(errno));
    close(fd);
    return false;
  }

  *timestamp = 0;

  close(fd);
  
  return true;
}

#ifdef USERFAULTFD
static int userfaultfd(int flags) { return syscall(SYS_userfaultfd, flags); }

struct thread_args {
  int fd;
  struct uffdio_range range;
};

static _Atomic uint64_t msg_received_tsc;

static void *userfaultfd_thread_routine(void *data) {
  size_t n;
  struct thread_args *args = (struct thread_args *)data;
  struct uffd_msg msg;
  struct uffdio_zeropage zeropage;

  for (;;) {
    n = read(args->fd, &msg, sizeof(msg));
    if (n == -1) {
      perror("read");
      break;
    } else if (n != sizeof(msg)) {
      fprintf(stderr, "Could only read %zd bytes from userfaultfd.\n", n);
      break;
    }

    atomic_store_explicit(&msg_received_tsc, rdtsc(), memory_order_relaxed);

    zeropage.range = args->range;
    zeropage.mode = 0;
    if (ioctl(args->fd, UFFDIO_ZEROPAGE, &zeropage) == -1) {
      perror("ioctl(UFFDIO_ZEROPAGE)");
      break;
    }
  }

  return NULL;
}
#endif

int main() {
  int ret = EXIT_SUCCESS;
  int tracepoints_dir_fd = -1;
  cpu_set_t cpu_set;
  ssize_t page_size;
  void *page = MAP_FAILED;
  int fd = -1;
  struct uffdio_api api_args = {0};
  struct uffdio_register register_args = {0};
  int err;
  uint64_t page_fault_tsc;
  uint32_t restore_state_start_tsc_1;
  uint32_t restore_state_start_tsc_2;
  uint32_t iret_tsc_1;
  uint32_t iret_tsc_2;
  uint64_t end_tsc;
  uint64_t restore_state_start_tsc;
  uint64_t iret_tsc;
  int read;
  struct timeval seen;
#ifdef USERFAULTFD
  struct thread_args thread_args;
  pthread_t userfaultfd_thread;
  bool started_userfaultfd_thread = false;
  struct uffd_msg msg;
  uint64_t msg_received_tsc_copy;
#endif

  tracepoints_dir_fd = open("/proc/sys/debug/fast-tracepoints", O_DIRECTORY | O_PATH);
  if (tracepoints_dir_fd == -1) {
    perror("open(/proc/sys/debug/fast-tracepoints)");
    ret = EXIT_FAILURE;
    goto out;
  }

  CPU_ZERO(&cpu_set);
  CPU_SET(0, &cpu_set);
  err = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
  if (err) {
    fprintf(stderr, "pthread_setaffinity_np: %s\n", strerror(err));
    ret = EXIT_FAILURE;
    goto out;
  }

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

#ifdef USERFAULTFD
  fd = userfaultfd(O_CLOEXEC);
  if (fd == -1) {
    perror("userfaultfd");
    ret = EXIT_FAILURE;
    goto out;
  }

  api_args.api = UFFD_API;
  if (ioctl(fd, UFFDIO_API, &api_args) == -1) {
    perror("ioctl(UFFDIO_API)");
    ret = EXIT_FAILURE;
    goto out;
  }

  register_args.range.start = (uint64_t)page;
  register_args.range.len = page_size;
  register_args.mode = UFFDIO_REGISTER_MODE_MISSING;
  if (ioctl(fd, UFFDIO_REGISTER, &register_args) == -1) {
    perror("ioctl(UFFDIO_REGISTER)");
    ret = EXIT_FAILURE;
    goto out;
  }

  thread_args.fd = fd;
  thread_args.range = register_args.range;
  err = pthread_create(&userfaultfd_thread, NULL, userfaultfd_thread_routine,
                       &thread_args);
  if (err) {
    fprintf(stderr, "pthread_create: %s\n", strerror(err));
    ret = EXIT_FAILURE;
    goto out;
  }

  started_userfaultfd_thread = true;

#ifdef TWO_CPUS
  CPU_ZERO(&cpu_set);
  CPU_SET(1, &cpu_set);
#endif
  err = pthread_setaffinity_np(userfaultfd_thread, sizeof(cpu_set), &cpu_set);
  if (err) {
    fprintf(stderr, "pthread_setaffinity_np: %s\n", strerror(err));
    ret = EXIT_FAILURE;
    goto out;
  }

  puts("save_state_start, save_state_end, read_lock_vma_start, "
       "read_lock_vma_end, walk_page_table_start, walk_page_table_end, "
       "wake_up_userfaultfd_start, wake_up_userfaultfd_end, "
       "handle_mm_fault_start, msg_received, handle_mm_fault_end, "
       "retry_read_lock_vma_start, retry_read_lock_vma_end, "
       "retry_walk_page_table_start, retry_walk_page_table_end, "
       "retry_handle_mm_fault_start, retry_handle_mm_fault_end, "
       "cleanup_start, cleanup_end, restore_state_start, iret, end");
#else
  puts("save_state_start, save_state_end, read_lock_vma_start, "
       "read_lock_vma_end, walk_page_table_start, walk_page_table_end, "
       "handle_mm_fault_start, handle_mm_fault_end, "
       "cleanup_start, cleanup_end, restore_state_start, iret, end");
#endif

  for (int i = 0; i < 100000; ++i) {
    page_fault_tsc = rdtsc();

    /* Trigger a page fault with the magic value in ebx. */
    asm volatile("movb (%[page]), %%dil"
                 : "=a"(iret_tsc_1), "=d"(iret_tsc_2),
                   "=b"(restore_state_start_tsc_1),
                   "=c"(restore_state_start_tsc_2)
                 : [page] "r"(page), "b"(0xb141a52a)
                 : "rdi");

    end_tsc = rdtsc();

    read = klogctl(SYSLOG_ACTION_READ_ALL, klog_buffer, klog_size);
    if (read == -1) {
      perror("klogctl(SYSLOG_ACTION_READ_ALL)");
      ret = EXIT_FAILURE;
      goto out;
    }

    sections = (struct sections){0};
    process_klog_buffer(klog_buffer, read, &sections, &seen);

    if ((sections.save_state.start_tsc == 0 &&
         sections.save_state.end_tsc == 0) ||
        (sections.read_lock_vma.start_tsc == 0 &&
         sections.read_lock_vma.end_tsc == 0) ||
        (sections.walk_page_table.start_tsc == 0 &&
         sections.walk_page_table.end_tsc == 0) ||
        (sections.handle_mm_fault.start_tsc == 0 &&
         sections.handle_mm_fault.end_tsc == 0) ||
        (sections.cleanup.start_tsc == 0 && sections.cleanup.end_tsc == 0)
#ifdef USERFAULTFD
        || (sections.wake_up_userfaultfd.start_tsc == 0 &&
            sections.wake_up_userfaultfd.end_tsc == 0) ||
        (sections.retry_read_lock_vma.start_tsc == 0 &&
         sections.retry_read_lock_vma.end_tsc == 0) ||
        (sections.retry_walk_page_table.start_tsc == 0 &&
         sections.retry_walk_page_table.end_tsc == 0) ||
        (sections.retry_handle_mm_fault.start_tsc == 0 &&
         sections.retry_handle_mm_fault.end_tsc == 0)
#endif
    ) {
      fputs("Missing timings in kernel log buffer.\n", stderr);
      munmap(page, page_size);
      free(klog_buffer);
      return EXIT_FAILURE;
    }

    restore_state_start_tsc =
        restore_state_start_tsc_1 | ((uint64_t)restore_state_start_tsc_2 << 32);
    iret_tsc = iret_tsc_1 | ((uint64_t)iret_tsc_2 << 32);

#ifdef USERFAULTFD
    msg_received_tsc_copy =
        atomic_load_explicit(&msg_received_tsc, memory_order_relaxed);
    if (msg_received_tsc_copy == 0) {
      fputs("userfaultfd thread was not notified.\n", stderr);
      ret = EXIT_FAILURE;
      goto out;
    }

    printf("%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 ", %" PRIu64 "\n",
           sections.save_state.start_tsc - page_fault_tsc,
           sections.save_state.end_tsc - page_fault_tsc,
           sections.read_lock_vma.start_tsc - page_fault_tsc,
           sections.read_lock_vma.end_tsc - page_fault_tsc,
           sections.walk_page_table.start_tsc - page_fault_tsc,
           sections.walk_page_table.end_tsc - page_fault_tsc,
           sections.wake_up_userfaultfd.start_tsc - page_fault_tsc,
           sections.wake_up_userfaultfd.end_tsc - page_fault_tsc,
           sections.handle_mm_fault.start_tsc - page_fault_tsc,
           msg_received_tsc_copy - page_fault_tsc,
           sections.handle_mm_fault.end_tsc - page_fault_tsc,
           sections.retry_read_lock_vma.start_tsc - page_fault_tsc,
           sections.retry_read_lock_vma.end_tsc - page_fault_tsc,
           sections.retry_walk_page_table.start_tsc - page_fault_tsc,
           sections.retry_walk_page_table.end_tsc - page_fault_tsc,
           sections.retry_handle_mm_fault.start_tsc - page_fault_tsc,
           sections.retry_handle_mm_fault.end_tsc - page_fault_tsc,
           sections.cleanup.start_tsc - page_fault_tsc,
           sections.cleanup.end_tsc - page_fault_tsc,
           restore_state_start_tsc - page_fault_tsc, iret_tsc - page_fault_tsc,
           end_tsc - page_fault_tsc);

    atomic_store_explicit(&msg_received_tsc, 0, memory_order_relaxed);
#else
    printf("%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 "\n",
           sections.save_state.start_tsc - page_fault_tsc,
           sections.save_state.end_tsc - page_fault_tsc,
           sections.read_lock_vma.start_tsc - page_fault_tsc,
           sections.read_lock_vma.end_tsc - page_fault_tsc,
           sections.walk_page_table.start_tsc - page_fault_tsc,
           sections.walk_page_table.end_tsc - page_fault_tsc,
           sections.handle_mm_fault.start_tsc - page_fault_tsc,
           sections.handle_mm_fault.end_tsc - page_fault_tsc,
           sections.cleanup.start_tsc - page_fault_tsc,
           sections.cleanup.end_tsc - page_fault_tsc,
           restore_state_start_tsc - page_fault_tsc, iret_tsc - page_fault_tsc,
           end_tsc - page_fault_tsc);
#endif

    if (madvise(page, page_size, MADV_DONTNEED) == -1) {
      perror("mmap");
      ret = EXIT_FAILURE;
      goto out;
    }
  }

out:
#ifdef USERFAULTFD
  if (started_userfaultfd_thread) {
    pthread_cancel(userfaultfd_thread);
    pthread_join(userfaultfd_thread, NULL);
  }
#endif
  if (fd != -1)
    close(fd);
  if (page)
    munmap(page, page_size);
  if (tracepoints_dir_fd != -1)
    close(tracepoints_dir_fd);

  return ret;
}
