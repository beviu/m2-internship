#include <stdio.h>
#include <stdlib.h>
#include <sys/swap.h>

#define SWAP_FLAG_SYNC_IO	0x80000 /* use synchronous writes */

int main(int argc, char **argv) {
  const char *arg0 = argv[0] ? argv[0] : "swapon-sync";

  if (argc != 2) {
    fprintf(stderr, "Usage: %s PATH\n", arg0);
    return EXIT_FAILURE;
  }
  
  if (swapon(argv[1], SWAP_FLAG_SYNC_IO) == -1) {
    perror("swapon");
    return EXIT_FAILURE;
  }
  
  return EXIT_SUCCESS;
}
