#include "../../../../include/usm/usm.h"

struct optEludeList {
  struct page *usmPage; // no pointer needed..
  struct list_head iulist;
  struct list_head proclist;
};

extern struct list_head freeList;
extern struct list_head usedList;

extern pthread_mutex_t policiesSet1Ulock;
extern pthread_mutex_t policiesSet1Flock;