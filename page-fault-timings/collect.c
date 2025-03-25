#define _GNU_SOURCE
#include <fcntl.h>
#include <inttypes.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#define SYSLOG_ACTION_READ_ALL 3
#define SYSLOG_ACTION_SIZE_BUFFER 10

static uint64_t rdtsc() {
  uint32_t eax, edx;

#ifndef RDTSC_UNSERIALIZED
  asm volatile("rdtsc" : "=a"(eax), "=d"(edx));
#else
  asm volatile("mfence; lfence\n"
               "rdtsc\n"
               "lfence"
               : "=a"(eax), "=d"(edx));
#endif

  return eax | ((uint64_t)edx << 32);
}

static int userfaultfd(int flags) { return syscall(SYS_userfaultfd, flags); }

enum parse_result {
  OK,
  ERROR,
  EARLY_END,
};

static enum parse_result
expect_character(const char **buffer, const char *buffer_end, char expected) {
  if (*buffer == buffer_end)
    return EARLY_END;

  return *(*buffer)++ == expected ? OK : ERROR;
}

static enum parse_result expect_string(const char **buffer,
                                       const char *buffer_end,
                                       const char *expected) {
  while (*expected != '\0') {
    if (*buffer == buffer_end)
      return EARLY_END;

    if (*(*buffer)++ != *expected++)
      return ERROR;
  }

  return OK;
}

static enum parse_result expect_delimited_field(const char **buffer,
                                                const char *buffer_end,
                                                char start, char end) {
  enum parse_result result = expect_character(buffer, buffer_end, start);
  if (result != OK)
    return result;

  for (;;) {
    if (*buffer == buffer_end)
      return EARLY_END;

    if (*(*buffer)++ == end)
      break;
  }

  return OK;
}

static enum parse_result read_u64(const char **buffer, const char *buffer_end,
                                  uint64_t *value) {
  if (*buffer == buffer_end)
    return EARLY_END;

  if (**buffer < '0' || **buffer > '9')
    return ERROR;

  *value = *(*buffer)++ - '0';

  while (*buffer != buffer_end && **buffer >= '0' && **buffer <= '9') {
    *value *= 10;
    *value += *(*buffer)++ - '0';
  }

  return OK;
}

static enum parse_result read_timestamp(const char **buffer,
                                        const char *buffer_end,
                                        struct timeval *timestamp) {
  uint64_t value;

  enum parse_result result = read_u64(buffer, buffer_end, &value);
  if (result != OK)
    return result;

  timestamp->tv_sec = value;

  result = expect_character(buffer, buffer_end, '.');
  if (result != OK)
    return result;

  result = read_u64(buffer, buffer_end, &value);
  if (result != OK)
    return result;

  timestamp->tv_usec = result;

  return OK;
}

static enum parse_result read_timestamp_part(const char **buffer,
                                             const char *buffer_end,
                                             struct timeval *timestamp) {
  enum parse_result result = expect_character(buffer, buffer_end, '[');
  if (result != OK)
    return result;

  /* Skip spaces. */
  for (;;) {
    if (*buffer == buffer_end)
      return EARLY_END;

    if (**buffer != ' ')
      break;

    ++(*buffer);
  }

  result = read_timestamp(buffer, buffer_end, timestamp);
  if (result != OK)
    return result;

  result = expect_character(buffer, buffer_end, ']');
  if (result != OK)
    return result;

  return expect_character(buffer, buffer_end, ' ');
}

struct log_line {
  const char *body;
  const char *body_end;
  struct timeval timestamp;
};

static enum parse_result read_log_line(const char **buffer,
                                       const char *buffer_end,
                                       struct log_line *line) {
  /* Optional field. */
  const char *tmp = *buffer;
  enum parse_result result = expect_delimited_field(&tmp, buffer_end, '<', '>');
  if (result == OK)
    *buffer = tmp;

  tmp = *buffer;
  result = read_timestamp_part(&tmp, buffer_end, &line->timestamp);
  if (result == OK) {
    *buffer = tmp;
  } else {
    line->timestamp.tv_sec = 0;
    line->timestamp.tv_usec = 0;
  }

  tmp = *buffer;
  result = expect_delimited_field(&tmp, buffer_end, '[', ']');
  if (result == OK) {
    result = expect_character(&tmp, buffer_end, ' ');
    if (result == OK)
      *buffer = tmp;
  }

  line->body = *buffer;

  for (;;) {
    if (*buffer == buffer_end)
      return EARLY_END;

    if (**buffer == '\n')
      break;

    ++(*buffer);
  }

  line->body_end = (*buffer)++;

  return OK;
}

struct section {
  uint64_t start_tsc;
  uint64_t end_tsc;
};

struct named_section {
  const char *name;
  const char *name_end;
  struct section section;
};

static enum parse_result read_named_section(const char **buffer,
                                            const char *buffer_end,
                                            struct named_section *section) {
  section->name = *buffer;

  for (;;) {
    if (*buffer == buffer_end)
      return EARLY_END;

    if (**buffer == ':')
      break;

    ++(*buffer);
  }

  section->name_end = (*buffer)++;

  enum parse_result result = expect_string(buffer, buffer_end, " start=");
  if (result != OK)
    return result;

  result = read_u64(buffer, buffer_end, &section->section.start_tsc);
  if (result != OK)
    return result;

  result = expect_string(buffer, buffer_end, ", end=");
  if (result != OK)
    return result;

  return read_u64(buffer, buffer_end, &section->section.end_tsc);
}

struct sections {
  struct section save_state;
  struct section read_lock_vma;
  struct section walk_page_table;
  struct section handle_mm_fault;
  struct section cleanup;
#ifdef USERFAULTFD
  struct section wake_up_userfaultfd;
  struct section retry_read_lock_vma;
  struct section retry_walk_page_table;
  struct section retry_handle_mm_fault;
#endif
};

static bool compare_section_name(const struct named_section *section,
                                 const char *with) {
  const char *name = section->name;

  while (*with != '\0') {
    if (name == section->name_end || *name++ != *with++)
      return false;
  }

  return name == section->name_end;
}

static void process_klog_buffer(const char *buffer, int length,
                                struct sections *sections,
                                struct timeval *seen) {
  const char *buffer_end = buffer + length;

  for (;;) {
    struct log_line line;

    const char *tmp = buffer;
    enum parse_result result = read_log_line(&tmp, buffer_end, &line);
    switch (result) {
    case OK:
      buffer = tmp;
      break;
    case ERROR:
      fprintf(stderr, "Failed to parse kernel log line: %s\n", buffer);
      return;
    case EARLY_END:
      return;
    }

    if (line.timestamp.tv_sec == 0 && line.timestamp.tv_usec == 0)
      continue;

    if (seen->tv_sec != 0 && seen->tv_usec != 0) {
      if ((line.timestamp.tv_sec < seen->tv_sec) ||
          (line.timestamp.tv_sec == seen->tv_sec &&
           line.timestamp.tv_usec <= seen->tv_usec))
        continue;
    }

    *seen = line.timestamp;

    struct named_section section;
    result = read_named_section(&line.body, line.body_end, &section);
    if (result == OK) {
      if (compare_section_name(&section, "save_state")) {
        sections->save_state = section.section;
      } else if (compare_section_name(&section, "read_lock_vma")) {
        sections->read_lock_vma = section.section;
      } else if (compare_section_name(&section, "walk_page_table")) {
        sections->walk_page_table = section.section;
      } else if (compare_section_name(&section, "handle_mm_fault")) {
        sections->handle_mm_fault = section.section;
      } else if (compare_section_name(&section, "cleanup")) {
        sections->cleanup = section.section;
      } else {
#ifdef USERFAULTFD
        if (compare_section_name(&section, "wake_up_userfaultfd")) {
          sections->wake_up_userfaultfd = section.section;
        } else if (compare_section_name(&section, "retry_read_lock_vma")) {
          sections->retry_read_lock_vma = section.section;
        } else if (compare_section_name(&section, "retry_walk_page_table")) {
          sections->retry_walk_page_table = section.section;
        } else if (compare_section_name(&section, "retry_handle_mm_fault")) {
          sections->retry_handle_mm_fault = section.section;
        }
#endif
      }
    }
  }
}

#ifdef USERFAULTFD
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
  cpu_set_t cpu_set;
  ssize_t page_size;
  int klog_size;
  char *klog_buffer = NULL;
  void *page = MAP_FAILED;
  int fd = -1;
  struct uffdio_api api_args = {0};
  struct uffdio_register register_args = {0};
  int err;
  struct sections sections;
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

  klog_size = klogctl(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);
  if (klog_size == -1) {
    perror("klogctl(SYSLOG_ACTION_SIZE_BUFFER)");
    ret = EXIT_FAILURE;
    goto out;
  }

  klog_buffer = malloc(klog_size);
  if (klog_buffer == NULL) {
    fputs("Failed to allocate memory for the kernel log buffer.\n", stderr);
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
  if (fd == -1)
    close(fd);
  if (page)
    munmap(page, page_size);
  if (klog_buffer)
    free(klog_buffer);

  return ret;
}
