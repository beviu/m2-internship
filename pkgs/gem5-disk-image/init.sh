#!/usr/bin/env bash

graceful_exit() {
  # Flush data to be transmitted to the serial port before exiting.
  perl -MPOSIX -e 'tcflush(0, TCOFLUSH)'
  sleep 10
  # The first m5 exit switches from KVM mode to full emulation mode, and
  # the second requests actually stopping Gem5.
  m5 exit
  m5 exit
}

mount -t proc proc /proc

action="$(m5 readfile)"

echo "Action: $action"

case "$action" in
  simple-mmap-test)
    echo "Starting simple-mmap-test..."
    SIMPLE_MMAP_TEST_M5_EXIT_BEFORE=1 SIMPLE_MMAP_TEST_M5_EXIT_AFTER=1 simple-mmap-test
    ;;
  simple-mmap-test-with-extmem-sigbus)
    echo "Starting simple-mmap-test with ExtMem (SIGBUS)..."
    LD_PRELOAD="$EXTMEM_PATH/lib/libextmem-default.so" EXTMEM_M5_EXIT=1 SIMPLE_MMAP_TEST_M5_EXIT_AFTER=1 simple-mmap-test
    ;;
  simple-mmap-test-with-extmem-ufault)
    echo "Starting simple-mmap-test with ExtMem (User Faults)..."
    LD_PRELOAD="$EXTMEM_UFAULT_PATH/lib/libextmem-default.so" EXTMEM_M5_EXIT=1 SIMPLE_MMAP_TEST_M5_EXIT_AFTER=1 simple-mmap-test
    ;;
  userfault-test)
    echo "Starting userfault-test..."
    m5 exit
    userfault-test
    m5 exit
    ;;
  *)
    echo "Unknown action." >&2
    graceful_exit
    ;;
esac

# Wait here for the m5 exit command to complete, instead of exiting the
# process which would cause a kernel panic.
sleep infinity
