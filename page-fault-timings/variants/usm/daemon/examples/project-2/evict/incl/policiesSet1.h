#include "../../alloc/incl/policiesSet1.h"

/*
    Sooo.. what to do here... -_-'
*/

#define workersByPagesThreshold 100

extern pthread_mutex_t policiesSet1Swaplock;
extern struct list_head swapList;

struct proc_swp_create_list_arg {
  int nr;
  struct list_head *procSwpLst;
};