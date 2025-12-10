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

action="$(m5 readfile)"

echo "Action: $action"

case "$action" in
  simple-mmap-test)
    echo "Starting simple-mmap-test..."
    SIMPLE_MMAP_TEST_M5_EXIT=1 simple-mmap-test
    ;;
  *)
    echo "Unknown action." >&2
    graceful_exit
    ;;
esac

# Wait here for the m5 exit command to complete, instead of exiting the
# process which would cause a kernel panic.
sleep infinity
