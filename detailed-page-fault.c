#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/klog.h>
#include <sys/mman.h>
#include <unistd.h>

#ifdef UNSERIALIZED_RDTSC
#include <x86intrin.h>
#endif

#define SYSLOG_ACTION_READ_ALL 3
#define SYSLOG_ACTION_SIZE_BUFFER 10

static uint64_t rdtsc_serialize() {
  uint32_t eax, edx;

  asm volatile("mfence; lfence\n"
               "rdtsc\n"
               "lfence"
               : "=a"(eax), "=d"(edx));

  return eax | ((uint64_t)edx << 32);
}

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
  struct section search_for_vma;
  struct section handle_mm_fault;
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
      } else if (compare_section_name(&section, "search_for_vma")) {
        sections->search_for_vma = section.section;
      } else if (compare_section_name(&section, "handle_mm_fault")) {
        sections->handle_mm_fault = section.section;
      }
    }
  }
}

int main() {
  ssize_t page_size = sysconf(_SC_PAGESIZE);
  if (page_size == -1) {
    perror("sysconf(_SC_PAGESIZE)");
    return EXIT_FAILURE;
  }

  int klog_size = klogctl(SYSLOG_ACTION_SIZE_BUFFER, NULL, 0);
  if (klog_size == -1) {
    perror("klogctl(SYSLOG_ACTION_SIZE_BUFFER)");
    return EXIT_FAILURE;
  }

  char *klog_buffer = malloc(klog_size);
  if (klog_buffer == NULL) {
    fputs("Failed to allocate memory for the kernel log buffer.\n", stderr);
    return EXIT_FAILURE;
  }

  void *page =
      mmap(NULL, page_size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (page == MAP_FAILED) {
    perror("mmap");
    return EXIT_FAILURE;
  }

  struct timeval seen;

  printf(
      "exception, save_state, search_for_vma, handle_mm_fault, restore_state, "
      "iret\n");

  for (int i = 0; i < 100000; ++i) {
    uint64_t page_fault_tsc;

#ifdef UNSERIALIZED_RDTSC
    page_fault_tsc = __rdtsc();
#else
    page_fault_tsc = rdtsc_serialize();
#endif

    uint32_t restore_state_start_tsc_1, restore_state_start_tsc_2, iret_tsc_1,
        iret_tsc_2;

    /* Trigger a page fault with the magic value in ebx. */
    asm volatile("movb (%[page]), %%dil"
                 : "=a"(iret_tsc_1), "=d"(iret_tsc_2),
                   "=b"(restore_state_start_tsc_1),
                   "=c"(restore_state_start_tsc_2)
                 : [page] "r"(page), "b"(0xb141a52a)
                 : "rdi");

    uint64_t end_tsc;

#ifdef UNSERIALIZED_RDTSC
    end_tsc = __rdtsc();
#else
    end_tsc = rdtsc_serialize();
#endif

    uint64_t restore_state_start_tsc =
        restore_state_start_tsc_1 | ((uint64_t)restore_state_start_tsc_2 << 32);
    uint64_t iret_tsc = iret_tsc_1 | ((uint64_t)iret_tsc_2 << 32);

    int read = klogctl(SYSLOG_ACTION_READ_ALL, klog_buffer, klog_size);
    if (read == -1) {
      perror("klogctl(SYSLOG_ACTION_READ_ALL)");
      munmap(page, page_size);
      free(klog_buffer);
      return EXIT_FAILURE;
    }

    struct sections sections = {0};
    process_klog_buffer(klog_buffer, read, &sections, &seen);

    if ((sections.save_state.start_tsc == 0 &&
         sections.save_state.end_tsc == 0) ||
        (sections.search_for_vma.start_tsc == 0 &&
         sections.search_for_vma.end_tsc == 0) ||
        (sections.handle_mm_fault.start_tsc == 0 &&
         sections.handle_mm_fault.end_tsc == 0)) {
      fputs("Missing timings in kernel log buffer.\n", stderr);
      munmap(page, page_size);
      free(klog_buffer);
      return EXIT_FAILURE;
    }

    printf("%" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64 ", %" PRIu64
           ", %" PRIu64 "\n",
           sections.save_state.start_tsc - page_fault_tsc,
           sections.save_state.end_tsc - sections.save_state.start_tsc,
           sections.search_for_vma.end_tsc - sections.search_for_vma.start_tsc,
           sections.handle_mm_fault.end_tsc -
               sections.handle_mm_fault.start_tsc,
           iret_tsc - restore_state_start_tsc, end_tsc - iret_tsc);

    if (madvise(page, page_size, MADV_DONTNEED) == -1) {
      perror("mmap");
      munmap(page, page_size);
      free(klog_buffer);
      return EXIT_FAILURE;
    }
  }

  munmap(page, page_size);
  free(klog_buffer);

  return 0;
}
