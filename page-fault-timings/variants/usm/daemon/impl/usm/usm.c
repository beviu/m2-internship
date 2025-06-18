#include "../../include/usm/usm.h"

void *usmZone = NULL;
struct page *pagesList = NULL;
int uffd, usmCMAFd, usmNewProcFd, usmFreePagesFd; //, usmTMapFd = 0;

struct processDesc *procDescList;

struct usm_global_ops global_ops;

unsigned long poolSize = 0;
unsigned int globalPagesNumber = 0;
unsigned int globalPageSize = 0;
unsigned int workersNumber = 0;
unsigned int lastChosenWorker = 0;
unsigned short policiesNumber = 0;
unsigned short allocPoliciesNumber = 0;
unsigned short swapPoliciesNumber = 0;
struct hashmap *usm_alloc_policies;
struct hashmap *usm_swap_policies;
char *allocAssignmentStrategy;
struct hashmap *usm_process_linking;
unsigned long allocPolicyThresholds[100];
int initThreshold = 0;
int init = 0;

pthread_t policiesWatcher;

void *usm_handle_policies_watcher() {
  while (1) {
    default_alloc_policy =
        *(struct usm_alloc_policy_ops *)
            allocPolicyThresholds[(usedMemory / poolSize) * 100];
#ifdef DEBUG
    printf("Applied policy at threshold %ld\n", (usedMemory / poolSize) * 100);
#endif
    sleep(15);
  }
}

int usm_policy_compare(const void *a, const void *b, void *udata) {
  const struct usm_policy *pa = a;
  const struct usm_policy *pb = b;
  return strcmp(pa->name, pb->name);
}

uint64_t usm_policy_hash(const void *item, uint64_t seed0, uint64_t seed1) {
  const struct usm_policy *policy = item;
  return hashmap_sip(policy->name, strlen(policy->name), seed0, seed1);
}

int usm_plink_compare(const void *a, const void *b, void *udata) {
  const struct usm_process_link *pa = a;
  const struct usm_process_link *pb = b;
  return strcmp(pa->procName, pb->procName);
}

uint64_t usm_plink_hash(const void *item, uint64_t seed0, uint64_t seed1) {
  const struct usm_process_link *plink = item;
  return hashmap_sip(plink->procName, strlen(plink->procName), seed0, seed1);
}

volatile unsigned long usedMemory = 0;
volatile unsigned long swappedMemory = 0;
unsigned long basePFN;

struct usm_worker usmWorkers;

struct usm_worker usmMemWorkers;
struct usm_worker usmMemWorkersWatch;

struct usm_watch_list usmMemWatchList;

#ifdef UTHREAD
void createUSMMemChannel(
    struct usm_channel_ops *ops, char *mem,
    struct usm_channel
        *workerChannelsList) { // return int to say whether full.. and add
                               // another worker if so? TODO applyOrNot // prio.
                               // toBeAdded!
  unsigned int usmid = *(unsigned int *)(mem + 4096 - sizeof(unsigned int));
  void *toUnmap = NULL;
#ifdef DEBUG
  printf("Dude.. id (creation) : %d\n", usmid);
#endif
  // getchar();
  struct usm_channel *tempChannel = &workerChannelsList[usmid]; /* malloc(sizeof(struct
  usm_channel)); TODO get back to this if (tempChannel==NULL) { retries++;
      if(retries<3)
          goto retryChn;
      printf("[Sys] Couldn't allocate memory for a new mem. channel node!
  Aborting.\n"); exit(1);
  }
  INIT_LIST_HEAD(&(tempChannel->iulist));*/
  tempChannel->usm_cnl_ops = ops;
  // tempChannel->buff=mem;                   freaking'.. .... bug.
  tempChannel->usm_cnl_type =
      RAM_BASED; // most unneeded anymore..      // and not changing nowadays so
                 // fix it up.. somehow TODO
  tempChannel->event.origin = __READ_ONCE(*(int *)mem);
  tempChannel->event.type = ALLOC; // actually stagnating to this now.. its
                                   // death implicitly taken care of..
  toUnmap = tempChannel->event.usmmem;
  tempChannel->event.usmmem =
      mem; // sort of duplicate?           | and heree.. for respecting our
           // logic, it ain't used at all.. hurts but meh, will TODO fix 'em
           // up..        // here.. and if state thingy and all.. try it out in
           // DEBUG just for fun... ^
#ifdef IUTHREAD
  if (*((unsigned int *)(mem + SYS_PAGE_SIZE - sizeof(unsigned int) * 3)) ==
      100) {
#ifdef DEBUG
    printf("[Info/%d] Special treater put up!", usmid);
#endif
    // *((unsigned int *)(mem+SYS_PAGE_SIZE-sizeof(unsigned int)*3)) = 0; //
    // temp. prol'ly.. Kernel guesses actual classic fast treatment when this is
    // still at that defined value.. so call classic ops on it on first fault
    // when not mod'ded, or do dis. TODO inv. | plus : lil' hack on usmTagger..
    tempChannel->usm_handle_evt = usm_handle_mem_special;
    // TODO add origin*.. get that from Majordomo and precharge it.         |
    // Ha! Even there at mem. ... dude, while trying to optimize everything ya
    // just ruining slickiness.. -_-'
    // tempChannel->usm_handle_evt=usm_handle_mem_evt;      // specializing TODO
    uthread_create_recursive(
        tempChannel->usm_cnl_id /**(int *)(((uintptr_t)mem)+4000)*/,
        usmid /*+1*/, NULL,
        *(int *)mem); // *(int *)mem=evt->origin; ... answer to 'em all.. hence
                      // even possible to move from here.. o_o' ; TODO,
                      // revolutionarize e'rything based off dis! ;''''D
#ifdef DEBUG
    printf("[Sys] RecUThread created for channel (mem) at %d! (vCPUid 0 (put "
           "kern. manChannel in zone and use..) for now)\n",
           usmid);
#endif
  } else {
    tempChannel->usm_handle_evt =
        usm_handle_mem; // TODO predo it.. now done at buff. level.     // this
                        // now even unneeded : uthread naught doing, could this.
    uthread_create(tempChannel->usm_cnl_id /**(int *)(((uintptr_t)mem)+4000)*/,
                   usmid /*+1*/,
                   NULL); // hence third arg. not needed      TODO rid
#ifdef DEBUG
    printf("[Sys] UThread created for channel (mem) at %d! (vCPUid 0 (put "
           "kern. manChannel in zone and use..) for now)\n",
           usmid);
#endif
  }
#else
  tempChannel->usm_handle_evt =
      usm_handle_mem; // TODO predo it.. now done at buff. level.     // this
                      // now even unneeded : uthread naught doing, could this.
  uthread_create(tempChannel->usm_cnl_id /**(int *)(((uintptr_t)mem)+4000)*/,
                 usmid /*+1*/,
                 NULL); // hence third arg. not needed      TODO rid
#ifdef DEBUG
  printf("[Sys] UThread created for channel (mem) at %d! (vCPUid 0 (put kern. "
         "manChannel in zone and use..) for now) (%d'Majordomo)\n",
         usmid, tempChannel->event.origin);
#endif
#endif
  __WRITE_ONCE(*(int *)(mem + sizeof(int)),
               0); // ! after dis !        // short for now dismissed
  // tempChannel->buff=mem;          // duplicate.. TODO rid either all
  // throughout..
  if (toUnmap)
    munmap(toUnmap, SYS_PAGE_SIZE);
}
#else
void createUSMMemChannel(
    struct usm_channel_ops *ops, char *mem,
    struct usm_channel
        *workerChannelsList) { // return int to say whether full.. and add
                               // another worker if so? TODO applyOrNot // prio.
                               // toBeAdded!
  unsigned int usmid = *(unsigned int *)(mem + 4096 - sizeof(unsigned int));
#ifdef DEBUG
  printf("Dude.. id (creation) : %d\n", usmid);
#endif
  // getchar();
  struct usm_channel *tempChannel = &workerChannelsList[usmid]; /* malloc(sizeof(struct
  usm_channel)); TODO get back to this if (tempChannel==NULL) { retries++;
      if(retries<3)
          goto retryChn;
      printf("[Sys] Couldn't allocate memory for a new mem. channel node!
  Aborting.\n"); exit(1);
  }
  INIT_LIST_HEAD(&(tempChannel->iulist));*/
  tempChannel->usm_cnl_ops = ops;
  // tempChannel->buff=mem;
  tempChannel->usm_cnl_type = RAM_BASED; // most unneeded anymore..
  tempChannel->event.usmmem = mem;       // sort of duplicate?
  tempChannel->usm_handle_evt =
      usm_handle_mem; // TODO predo it.. now done at buff. level.
  __WRITE_ONCE(*(int *)(mem + sizeof(int)), 0);
  // TODO add origin*.. get that from Majordomo and precharge it.
  // tempChannel->usm_handle_evt=usm_handle_mem_evt;      // specializing TODO
#ifdef DEBUG
  printf("[Sys] Channel appended (mem) at %d!\n", usmid);
#endif
}
#endif

void createUSMChannel(struct usm_channel_ops *ops,
                      int channelFd) { // prio. toBeAdded!
  pthread_t *singMajordomo = (pthread_t *)malloc(sizeof(pthread_t));
  int retries = 0;
  pthread_t policiesWatcher;
  // unsigned int usmid;  // second main diff. (af. comm.)
retryChn:
  struct usm_channel *tempChannel = (struct usm_channel *)malloc(
      sizeof(struct usm_channel)); // not really nec. (like, not here..)
  if (tempChannel == NULL) {
    retries++;
    if (retries < 3)
      goto retryChn;
    printf("[Sys] Couldn't allocate memory for a new mem. channel node! "
           "Aborting.\n");
    exit(1);
  }

  tempChannel->usm_cnl_ops = ops;
  tempChannel->fd = channelFd;
  tempChannel->usm_cnl_type = FD_BASED; // most unneeded anymore..
  // ("not here" up there.. maybe link it back to the channel.. so that we can
  // keep statuses of 'em in a bigger scope)

  pthread_create(
      singMajordomo, NULL, usm_handle_fd,
      (void *)tempChannel); // tempChannel->usm_handle_evt=usm_handle_fd;
#ifdef DEBUG
  printf("[Sys] Majordomo launched!\n");
#endif
}

void appendUSMChannelLegacy(
    struct usm_channel_ops *ops, int channelFd,
    struct usm_channel *workerChannelsList) { // prio. toBeAdded!
  struct usm_channel *tempChannel = &workerChannelsList[0];
  INIT_LIST_HEAD(&(tempChannel->iulist));
  tempChannel->usm_cnl_ops = ops;
  tempChannel->fd = channelFd;
  tempChannel->usm_cnl_type = FD_BASED;
#ifdef DEBUG
  printf("[Sys] Channel appended! [Legacy]\n");
#endif
}

void appendUSMChannel(struct usm_channel_ops *ops, int channelFd,
                      struct list_head *workersChannelList,
                      pthread_mutex_t *chnmutex) { // prio. toBeAdded!
  int retries = 0;
retryChn:
  struct usm_channel *tempChannel = malloc(sizeof(struct usm_channel));
  if (tempChannel == NULL) {
    retries++;
    if (retries < 3)
      goto retryChn;
    printf(
        "[Sys] Couldn't allocate memory for a new channel node! Aborting.\n");
    exit(1);
  }
  INIT_LIST_HEAD(&(tempChannel->iulist));
  tempChannel->usm_cnl_ops = ops;
  tempChannel->fd = channelFd;
  tempChannel->usm_cnl_type = FD_BASED;
  pthread_mutex_lock(chnmutex);
  list_add(&(tempChannel->iulist), workersChannelList);
  pthread_mutex_unlock(chnmutex);
#ifdef DEBUG
  printf("[Sys] Channel appended!\n");
#endif
}
/* TODO fuse these two.. */
void appendUSMMemChannel(struct usm_channel_ops *ops, char *mem,
                         struct list_head *workersChannelList,
                         pthread_mutex_t *chnmutex) { // prio. toBeAdded!
  int retries = 0;
retryChn:
  struct usm_channel *tempChannel = malloc(sizeof(struct usm_channel));
  if (tempChannel == NULL) {
    retries++;
    if (retries < 3)
      goto retryChn;
    printf("[Sys] Couldn't allocate memory for a new mem. channel node! "
           "Aborting.\n");
    exit(1);
  }
  INIT_LIST_HEAD(&(tempChannel->iulist));
  tempChannel->usm_cnl_ops = ops;
  // tempChannel->buff=mem;
  tempChannel->usm_cnl_type = RAM_BASED;
  pthread_mutex_lock(chnmutex);
  list_add(&(tempChannel->iulist), workersChannelList);
  pthread_mutex_unlock(chnmutex);
#ifdef DEBUG
  printf("[Sys] Channel appended (mem)!\n");
#endif
}

int usm_init(unsigned long poolSize, unsigned int pageSize) {
  srand(time(NULL));
  procDescList = malloc(MAX_PROC * sizeof(struct processDesc));
  uffd = syscall(__NR_userfaultfd,
                 O_CLOEXEC /*| O_NONBLOCK weird, not wanting now..*/);
  int res = 0;
  // cpu_set_t cpuset;
  if (uffd == -1) {
    perror("syscall/userfaultfd");
    return errno;
  }
  struct uffdio_api uffdio_api;
  uffdio_api.api = UFFD_API;
  uffdio_api.features = 0;
  if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1) {
    perror("\t\t[Mayday] UFFD_API ioctl failed! Exiting.");
    return errno;
  }
  usmMemWorkers.com = mmap(
      NULL, SYS_PAGE_SIZE, PROT_READ | PROT_WRITE, /*MAP_FIXED |*/ MAP_SHARED,
      uffd /* doesn't matter which for now.. |now does.. instances management*/,
      0);
  if (usmMemWorkers.com == MAP_FAILED) {
    perror("Hold up!\n");
    getchar();
    abort();
  }
  /*#ifdef UINTR
      if (uintr_register_handler(waker_handler_local, 0) < 0) {
          perror("reg_hand_uipi ");
          abort();
      }
      __WRITE_ONCE(*(int *)((uintptr_t)(usmMemWorkers.com)+2048+sizeof(int)),
  uintr_create_fd(0, 0)); #endif
      __WRITE_ONCE(*(int *)((uintptr_t)(usmMemWorkers.com)+2048), getpid());
  #ifdef DEBUG
          printf("[Sys] Wrote pid %d, %d!\n", getpid(), *(int
  *)((uintptr_t)(usmMemWorkers.com)+2048)); #endif*/
  // .. maybe the thread's number... but meh, we could just look for the ones
  // needed to boom up when receiving that signal..
  // .. with smthn like __WRITE_ONCE(*(usmMemWorkers.com+2048+4), 0 /*thread
  // number (boss giving floors to uthreads)*/);
usmCMAinit:
  usmCMAFd = open("/dev/USMMcd", O_RDWR);
  if (usmCMAFd < 0) {
    printf("[Mayday] CMA untakable! Check USM's module. Try again?\n");
    getchar();
    goto usmCMAinit;
  }
/*usmTMapinit:
    usmTMapFd=open("/dev/USMMTcd", O_RDWR);
    if(usmCMAFd<0) {
        printf("[Mayday] Thread map fd untakable! Check USM's module. Try
   again?\n"); getchar(); goto usmTMapinit;
    }*/
usmFPInit:
  usmFreePagesFd = open("/proc/usmPgs", O_RDWR);
  if (usmFreePagesFd < 0) {
    printf("[Mayday] Free pages unseeable! Check USM's module. Try again?\n");
    getchar();
    goto usmFPInit;
  }
usmNPInit:
  usmNewProcFd = open("/proc/usm", O_RDWR);
  if (usmNewProcFd < 0) {
    printf("[Mayday] New processes unreceivable! Check USM's module. Try "
           "again?\n");
    getchar();
    goto usmNPInit;
  }
  globalPagesNumber =
      poolSize / SYS_PAGE_SIZE; // globalPageSize somehow later on for different
                                // sized pages' forming..
#ifdef DEBUG
  printf("[Sys] Pool size : %ldB | %.2fKB | %.2fMB\n", poolSize,
         (float)poolSize / 1024, (float)poolSize / 1024 / 1024);
  printf("[Sys] Global pages' number : %u\n", globalPagesNumber);
#endif
  usmZone = mmap(NULL, poolSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED,
                 usmCMAFd, 0);
  if (usmZone == MAP_FAILED) {
#ifdef DEBUG
    perror("[Sys] mmap failed!");
#endif
    return errno;
  }
#ifdef DEBUG
  else
    printf("[Sys] Mapped!\n");
#endif
  memset(usmZone, 0, poolSize); // toComment
#ifdef DEBUG
  printf("[Sys] Zeroed!\n");
#endif
  basePFN = usm_get_pfn(uffd, (unsigned long)usmZone,
                        (unsigned long)globalPagesNumber, 0);
  if (basePFN == 1) {
#ifdef DEBUG
    printf("[Sys] Couldn't get first PFN!\n");
#endif
    return 1;
  }
  pagesList = (struct page *)malloc(sizeof(struct page) * globalPagesNumber);
  for (unsigned int i = 0; i < globalPagesNumber; i++)
    insertPage(&pagesList, basePFN + i,
               (intptr_t)(usmZone + (i * SYS_PAGE_SIZE)), i);
/*
    unsigned int chunksNumber = poolSize/USM_CHUNK_SIZE;
    usmZone=mmap(NULL, chunksNumber?USM_CHUNK_SIZE:poolSize,
PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, usmCMAFd, 0);              //
usmZone & poolSize needn't be global. if(usmZone==MAP_FAILED) { perror("[Sys]
mmap failed! Aborting."); exit(1);
    }
    memset(usmZone, 0, chunksNumber?USM_CHUNK_SIZE:poolSize);
    basePFN = usm_get_pfn(uffd,(unsigned long)usmZone,(unsigned
long)globalPagesNumber); if (basePFN==1) { #ifdef DEBUG printf("[Sys] Couldn't
get first PFN!\n"); #endif return 1;
    }
    unsigned int tempPos = 0;
nextChunk:
    for (unsigned int i = tempPos;
i<(chunksNumber?USM_CHUNK_SIZE/SYS_PAGE_SIZE:(poolSize%USM_CHUNK_SIZE)/SYS_PAGE_SIZE);
i++) insertPage(&pagesList, basePFN+i, (intptr_t)(usmZone+(i*SYS_PAGE_SIZE)),
i); if (--chunksNumber>0 || (--chunksNumber==0 && poolSize%USM_CHUNK_SIZE)) {
        usmZone=mmap(NULL, chunksNumber?USM_CHUNK_SIZE:poolSize%USM_CHUNK_SIZE,
PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, usmCMAFd, 0);
        if(usmZone==MAP_FAILED) {
            perror("[Sys] Subsequent mmap failed! Aborting.");
            exit(1);
        }
        memset(usmZone, 0, chunksNumber?USM_CHUNK_SIZE:poolSize%USM_CHUNK_SIZE);
        tempPos+=USM_CHUNK_SIZE/SYS_PAGE_SIZE;
        goto nextChunk;
    }
*/
#ifdef DEBUG
  printf("[Sys] Inserted PFNs range : %lu | %lu \n", pagesList->physicalAddress,
         (pagesList + globalPagesNumber - 1)->physicalAddress);
#endif
#ifdef DEBUG
  printf("[Sys] USM page table initialized!\n");
#endif
  struct usm_worker *workersIterator = &usmWorkers;
  workersNumber = 0; // compat. new des. TODO temp.
  workersNumber += 2;
#ifdef DEBUG
  printf("[Sys] Number of workers : %d (forced to %d)\n", workersNumber - 2,
         workersNumber);
#endif
  for (int i = 0; i < workersNumber; i++) {
    // INIT_LIST_HEAD(&(workersIterator->usm_channels));
    // INIT_LIST_HEAD(&(workersIterator->usm_current_events));
    // workersIterator->usm_current_events_length=0;
    // pthread_mutex_init(&workersIterator->chnmutex, NULL);
    pthread_attr_init(&workersIterator->attrs);
    workersIterator->usm_wk_ops = malloc(sizeof(struct usm_worker_ops));
    workersIterator->usm_wk_ops->usm_handle_evt = usm_handle_events;
    // workersIterator->usm_wk_ops->usm_sort_evts=usm_sort_events;
    workersIterator->thread_name = (char *)malloc(260);
    bzero(workersIterator->thread_name, 260);
    sprintf(workersIterator->thread_name, "Worker-%d", i);
    /*CPU_ZERO(&cpuset);
    CPU_SET(i+1, &cpuset);

    if (pthread_attr_setaffinity_np(&workersIterator->attrs, sizeof(cpu_set_t),
    &cpuset) != 0) { perror("pthread_setaffinity_np"); exit(1);
    }*/

    if (i == workersNumber - 1) {
      sprintf(workersIterator->thread_name +
                  strlen(workersIterator->thread_name),
              "/freePagesReceiver");
      // appendUSMChannel(&usm_cnl_freed_ops, usmFreePagesFd,
      // &(workersIterator->usm_channels), &workersIterator->chnmutex);
      appendUSMChannelLegacy(
          &usm_cnl_freed_ops, usmFreePagesFd,
          &(workersIterator
                ->usm_channels)); // workersIterator->usm_channels[0]=&usm_cnl_freed_ops;
    }
    if (i == workersNumber - 2) {
      sprintf(workersIterator->thread_name +
                  strlen(workersIterator->thread_name),
              "/processesReceiver");
      // appendUSMChannel(&usm_cnl_new_process_ops, usmNewProcFd,
      // &(workersIterator->usm_channels), &workersIterator->chnmutex);
      appendUSMChannelLegacy(&usm_cnl_new_process_ops, usmNewProcFd,
                             &(workersIterator->usm_channels));
    }
#ifdef DEBUG
    printf("[Sys] Init'ed worker %s!\n", workersIterator->thread_name);
#endif
    /*if(list_empty(&(workersIterator->usm_channels)))      // new but totally
    legacy.. former test purposes.. printf("One empty.. %p\n",
    &(workersIterator->usm_channels)); else printf("One pop'ed.... %p\n",
    &(workersIterator->usm_channels));*/
    if (i < workersNumber - 1) {
      workersIterator->next = malloc(sizeof(struct usm_worker));
      workersIterator = workersIterator->next;
    } else
      workersIterator->next = NULL;
#ifdef DEBUG
    printf("[Sys] One worker pushed!\n");
#endif
  }
  workersNumber -= 2; // meh..        // now just those two + vCPUsNumber

  pthread_attr_init(&usmMemWorkers.attrs);
  usmMemWorkers.id = 0;
  usmMemWorkers.usm_wk_ops = malloc(sizeof(struct usm_worker_ops));
  usmMemWorkers.usm_wk_ops->usm_handle_evt = usm_handle_events;
  usmMemWorkers.thread_name = (char *)malloc(260);
  usmMemWorkers.state = (bool *)((uintptr_t)(usmMemWorkers.com) + 4092);
#ifdef DEBUG
  printf("[Sys] state : %d\n", *((&usmMemWorkers)->state));
#endif
  *(usmMemWorkers.state) = true;
#ifdef DEBUG
  printf("[Sys] state : %d\n", *((&usmMemWorkers)->state));
#endif
  for (int i = 0; i < MAX_UTHREADS; i++) {
    usmMemWorkers.usm_channels[i].usm_cnl_id = usmMemWorkers.id;
  }
  // following should be done inside the thread.. but yeah, uthreads' fixing
  // attempts yet again.. usmMemWorkers.com = mmap(NULL, SYS_PAGE_SIZE,
  // PROT_READ|PROT_WRITE, /*MAP_FIXED |*/ MAP_SHARED, uffd  /* doesn't matter
  // which for now.. |now does.. instances management*/, 0);
  /*if(!usmMemWorkers.com) {
      printf("Hold up!\n");
      getchar();
  }*/
  /**/
  usmMemWatchList.com = usmMemWorkers.com;
  usmMemWatchList.state = usmMemWorkers.state;
  usmMemWatchList.thread_id = &usmMemWorkers.thread_id; // TODO, temporary!
  usmMemWatchList.next = NULL;
  bzero(usmMemWorkers.thread_name, 260);
  sprintf(usmMemWorkers.thread_name, "memWorker-%d", 0);
  sprintf(usmMemWorkers.thread_name + strlen(usmMemWorkers.thread_name),
          "/bravoZero");
  /**/
  // pthread_attr_init(&usmMemWorkers.attrs);
  /*usmMemWorkers.usm_wk_ops=malloc(sizeof(struct usm_worker_ops));
  usmMemWorkers.usm_wk_ops->usm_handle_evt=usm_handle_events;*/
#ifdef DEBUG
  printf("[Sys] Init'ed memWorker %s!\n", usmMemWorkers.thread_name);
#endif
#ifdef DEBUG
  printf("[Sys] memWorker pushed!\n");
#endif

  usmMemWorkersWatch.thread_name = (char *)malloc(260);
  bzero(usmMemWorkersWatch.thread_name, 260);
  sprintf(usmMemWorkersWatch.thread_name, "watchWorker");
  sprintf(usmMemWorkersWatch.thread_name +
              strlen(usmMemWorkersWatch.thread_name),
          "/maydayOne");
#ifdef DEBUG
  printf("[Sys] Init'ed watchWorker %s!\n", usmMemWorkersWatch.thread_name);
#endif
#ifdef DEBUG
  printf("[Sys] watchWorker pushed!\n");
#endif

  int procHolderIndex = 0;
  while (procHolderIndex < MAX_PROC) {
    procDescList[procHolderIndex].alloc_policy = &default_alloc_policy;
    procDescList[procHolderIndex].swap_policy = &default_swap_policy;
    procDescList[procHolderIndex].allocated = 0;
    procDescList[procHolderIndex].swapped = 0;
    INIT_LIST_HEAD(&(procDescList[procHolderIndex].swapList));
    INIT_LIST_HEAD(&(procDescList[procHolderIndex].usedList));
    INIT_LIST_HEAD(&(procDescList[procHolderIndex].swapCache));
    procDescList[procHolderIndex].prio = 0;
    pthread_mutex_init(&procDescList[procHolderIndex].tMapLock, NULL);
    pthread_mutex_init(&procDescList[procHolderIndex].lock, NULL);
    pthread_mutex_init(&procDescList[procHolderIndex].alcLock, NULL);
    pthread_mutex_init(&procDescList[procHolderIndex].swpLock, NULL);
    procHolderIndex++;
  }
  usm_alloc_policies =
      hashmap_new(sizeof(struct usm_policy), 0, 0, 0, usm_policy_hash,
                  usm_policy_compare, NULL, NULL);
  usm_swap_policies =
      hashmap_new(sizeof(struct usm_policy), 0, 0, 0, usm_policy_hash,
                  usm_policy_compare, NULL, NULL);
  usm_process_linking =
      hashmap_new(sizeof(struct usm_process_link), 0, 0, 0, usm_plink_hash,
                  usm_plink_compare, NULL, NULL);
  init = 1;
  return 0;
}

int usm_set_alloc_policy_assignment_strategy(
    char *alloc_policy_assignment_strategy_cfg) {
  int ret = 0;
  FILE *fp = fopen(alloc_policy_assignment_strategy_cfg, "r"); // w temp. to er.
  if (!fp) {
#ifdef DEBUG
    perror("[Sys] Cannot open specified file");
#endif
    ret = 1;
  }
  char ln[1000];
  char arg1[20];
  while (fgets(ln, sizeof(ln), fp)) {
    if (ln[0] == '#' || sscanf(ln, "%s ", arg1) <= 0)
      continue;
    if (!strcmp(arg1, "memory")) {
      char arg2[20];
      sscanf(ln + strlen(arg1), "%s", arg2);
      poolSize = atoi(arg2);
      /*if(poolSize>MAX_USM_SIZE) {               TODO just communicate
availabilities seamlessly... from the module #ifdef DEBUG printf("[Sys] Maximum
available memory : %uMB.\n[Sys] Aborting.\n",MAX_USM_SIZE); #endif break;
      }*/
#ifdef DEBUG
      printf("[Sys] [Conf] Memory : %ldMB\n", poolSize);
#endif
      poolSize *= 1024 * 1024;
      continue;
    }
    if (!strcmp(arg1, "workers")) {
      char arg2[20];
      sscanf(ln + strlen(arg1), "%s", arg2);
      workersNumber = atoi(arg2);
      workersNumber = 0; // temp. compliant state.
#ifdef DEBUG
      printf("[Sys] [Conf] Workers : %d\n", workersNumber);
#endif
      continue;
    }
    if (!strcmp(arg1, "page")) {
      char arg2[20];
      sscanf(ln + strlen(arg1), "%s", arg2);
      globalPageSize = atoi(arg2);
#ifdef DEBUG
      printf("[Sys] [Conf] Page : %dB\n", globalPageSize);
#endif
      if (!init)
        goto temp;
      continue;
    }
    if (!strcmp(arg1, "threshold")) {
      char arg2[20], arg3[20];
      struct usm_policy *alloc_policy;
      sscanf(ln + strlen(arg1), "%s %s", arg2, arg3);
#ifdef DEBUG
      printf("[Sys] threshold, policy : %s | %s\n", arg2, arg3);
#endif
      if (!initThreshold) {
        int idx = 0;
        while (idx < 100)
          allocPolicyThresholds[idx++] = (unsigned long)&default_alloc_policy;
        initThreshold++;
      }
      alloc_policy = (struct usm_policy *)hashmap_get(
          usm_alloc_policies, &(struct usm_policy){.name = arg3});
      if (!alloc_policy) {
#ifdef DEBUG
        printf("[Sys] Couldn't find %s policy, please check provided "
               "configuration file.\n",
               arg3);
#endif
        break;
      }
#ifdef DEBUG
      else
        printf("[Sys] Found %s policy.\n", alloc_policy->name);
#endif
      int tempPerc = atoi(arg2);
      while (tempPerc < 100)
        allocPolicyThresholds[tempPerc++] = alloc_policy->ops;
      continue;
    }
    if (!strcmp(arg1, "process")) {
      char arg2[20];
      sscanf(ln + strlen(arg1), "%s", arg2);
      if (strcmp(arg2, "swap") && strcmp(arg2, "alloc")) {
#ifdef DEBUG
        printf("Unknown process configuration argument : %s\n", arg2);
#endif
        break;
      }
      char arg3[20], arg4[20];
      struct usm_policy *policy;
      sscanf(ln + strlen(arg1) + strlen(arg2) + 1, "%s %s", arg3, arg4);
#ifdef DEBUG
      printf("[Sys] Process :  %s | %s\n", arg3, arg4);
#endif
      if (!strcmp(arg2, "alloc"))
        policy = (struct usm_policy *)hashmap_get(
            usm_alloc_policies, &(struct usm_policy){.name = arg4});
      else
        policy = (struct usm_policy *)hashmap_get(
            usm_swap_policies, &(struct usm_policy){.name = arg4});
      if (!policy) {
#ifdef DEBUG
        printf("[Sys] Couldn't find %s policy, please check provided "
               "configuration file.\n",
               arg4);
#endif
        break;
      }
#ifdef DEBUG
      else
        printf("[Sys] Found %s | %s policy #%s\n", arg3, policy->name, arg2);
#endif
      struct usm_process_link *plink = (struct usm_process_link *)hashmap_get(
          usm_process_linking, &(struct usm_process_link){.procName = arg3});
      if (plink) {
#ifdef DEBUG
        printf("[Sys] Appending to found policy for %s.\n", plink->procName);
#endif
      } else {
        plink =
            (struct usm_process_link *)malloc(sizeof(struct usm_process_link));
        plink->procName = malloc(strlen(arg3));
        strcpy(plink->procName, arg3);
        plink->prio = 0;
      }
      if (!strcmp(arg2, "alloc"))
        plink->alloc_pol = policy->ops;
      else
        plink->swp_pol = policy->ops;
#ifdef DEBUG
      printf("Adding found policy definition to |%s|\n",
             (&(struct usm_process_link){.procName = arg3,
                                         .alloc_pol = policy->ops})
                 ->procName);
#endif
      if (!hashmap_set(usm_process_linking, plink)) {
        if (hashmap_oom(usm_process_linking)) {
#ifdef DEBUG
          printf("[Sys] Couldn't allocate new hashmap member's memory.\n");
#endif
          break;
        }
        struct usm_process_link *ct;
        ct = (struct usm_process_link *)hashmap_get(
            usm_process_linking, &(struct usm_process_link){.procName = arg3});
        if (!ct) {
#ifdef DEBUG
          printf("Wut in the..\n");
#endif
          break;
        }
#ifdef DEBUG
        printf("Overwritten |%s|'s policy definition with %s %s policy\n",
               ct->procName, policy->name, arg2);
#endif
      }
      continue;
    }
    if (!strcmp(arg1, "priority")) { // priorities should always come after the
                                     // policies binding in the cfg file..
      char arg2[20], arg3[20];
      struct usm_process_link *plink;
      sscanf(ln + strlen(arg1), "%s %s", arg2, arg3);
#ifdef DEBUG
      printf("[Sys] Priority :  %s | %s\n", arg2, arg3);
#endif
      plink = (struct usm_process_link *)hashmap_get(
          usm_process_linking, &(struct usm_process_link){.procName = arg2});
      if (plink) {
#ifdef DEBUG
        printf("[Sys] Found preexisting %s with %lu policy.\n", arg2,
               plink->alloc_pol); // TODO ..hmm....
#endif
      } else {
        plink = malloc(sizeof(struct usm_process_link));
        plink->procName = malloc(strlen(arg2));
        plink->alloc_pol = 0;
        plink->swp_pol = 0;
        strcpy(plink->procName, arg2);
      }
      plink->prio = atoi(arg3);
#ifdef DEBUG
      printf("Adding : |%s|\t%s\n", arg2, arg3);
#endif
      if (!hashmap_set(
              usm_process_linking,
              plink)) { //&(struct
                        //usm_process_link){.procName=arg2,.pol=/*alloc_policy->ops*/arg3})){
        if (hashmap_oom(usm_process_linking)) {
#ifdef DEBUG
          printf("[Sys] Couldn't allocate new hashmap member's memory.\n");
#endif
          break;
        }
        struct usm_process_link *ct;
        ct = (struct usm_process_link *)hashmap_get(
            usm_process_linking,
            plink); // &(struct usm_process_link){.procName=arg2}
        if (!ct) {
#ifdef DEBUG
          printf("Wut in the..\n");
#endif
          break;
        }
#ifdef DEBUG
        printf("Overwritten |%s|'s policy definition with priority %d.\n",
               ct->procName, plink->prio);
#endif
      }
      continue;
    }
#ifdef DEBUG
    printf("Unknown configuration argument : %s\n", arg1);
#endif
    break;
    // memset(arg1, 0, sizeof(arg1));
  }
  if (!feof(fp)) {
#ifdef DEBUG
    printf("[Sys] Couldn't reach end of config. file.\n");
#endif
    ret = 1;
  }
  if (globalPageSize ==
      0) // round it to the closest lower (bigger?) multiple.. TODO
    globalPageSize = SYS_PAGE_SIZE;
temp:
  fclose(fp);
  return ret;
}

/*
    Parse les arguments et initialise des variables globales
*/
void usm_parse_args(char *argv[], int argc) {
  if (argc < 2) {
    printf("[Sys] Please specify config. file location...\n");
    exit(1);
  }
  allocAssignmentStrategy = argv[1];
  if (usm_set_alloc_policy_assignment_strategy(allocAssignmentStrategy)) {
    printf("[Sys] Failed to assign strategy ; aborting.\n");
    exit(1);
  }
}

void dead(int sig) {
  (void)sig;
  munmap(usmZone, poolSize);
#ifdef DEBUG
  printf("iaossu!\n");
#endif
  abort(); // exit..
}

void usm_launch(struct usm_global_ops globOps) {
  struct usm_ops *alloc_usm = globOps.dev_usm_alloc_ops;
  struct usm_ops *swap_usm = globOps.dev_usm_swap_ops;
  struct sigaction sa;
  sa.sa_flags = 0;
  sa.sa_sigaction = (void *)dead;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGINT);
  sigaction(SIGINT, &sa, NULL);
  global_ops = globOps;
  if (usm_init(poolSize, globalPageSize)) {
    printf("[Sys] Couldn't initialize USM.\n");
    exit(1);
  }
  // if not NULL... but dev. should know it can't..hmmm..
  if (!alloc_usm || !swap_usm) {
    printf("[Sys] Provided NULL setup(s).\n");
    exit(1);
  }
  if (alloc_usm->usm_setup(globalPagesNumber)) {
    printf("[Sys] Failed to apply provided alloc. setup.\n");
    exit(1);
  }
  if (swap_usm->usm_setup(globalPagesNumber)) {
    printf("[Sys] Failed to apply provided swap. setup.\n");
    exit(1);
  }
  if (usm_set_alloc_policy_assignment_strategy(allocAssignmentStrategy)) {
    printf("[Sys] Failed to assign strategy ; aborting.\n");
    exit(1);
  }
  /*struct usm_worker *workersIterator = &usmWorkers;
  int wk_nr = 0;
  // TODO : check on default policies if init'ed ... either here or in init
(set_assignment_strat.) while(workersIterator) {
      // if..type...poll....else.
      // pthread_create(&(workersIterator->thread_id),
&(workersIterator->attrs), usm_handle_evt_poll, (void*)workersIterator);   //
rep. by :
      // uthread  .. the freaking whole loop, by in sched.c
      workersIterator=workersIterator->next;
#ifdef DEBUG
      printf("Worker %d launched!\n", wk_nr);
#endif
      wk_nr++;
      if(wk_nr==workersNumber+1)
          break;
  }*/
  /*sigset_t set, old;
      sigemptyset(&set);
      sigaddset(&set, SIGUSR1);
      sigprocmask(SIG_UNBLOCK, &set, &old);
#ifdef DEBUG
      printf("Sig.s globally set!\n");
#endif*/

  /*struct usm_worker *workersIterator = &usmWorkers;
  int wk_nr = 0;
  // TODO : check on default policies if init'ed ... either here or in init
(set_assignment_strat.) while(workersIterator) {
      // if..type...poll....else.
      pthread_create(&(workersIterator->thread_id), NULL , usm_handle_evt_poll,
(void*)workersIterator); //&(workersIterator->attrs)   // rep. by :
      // uthread  .. the freaking whole loop, by in sched.c
      workersIterator=workersIterator->next;
#ifdef DEBUG
      printf("Worker %d launched!\n", wk_nr);
#endif
      wk_nr++;
      if(wk_nr==workersNumber+1)
          break;
  }
  pthread_create(&(usmMemWorkers.thread_id), NULL , usm_mem_worker_handler,
(void*)&usmMemWorkers);  //&(workersIterator->attrs)
  pthread_create(&(usmMemWorkersWatch.thread_id), NULL, usm_mem_workers_waker,
(void*)&usmMemWatchList);    //&(workersIterator->attrs)*/

  setSched();

  // munmap(usmZone, poolSize);      // temp. zone/loc.!  ..ain't even sensy...
  // sig..

  /*if (initThreshold)        TODO : put back in.
      pthread_create(&policiesWatcher,NULL,usm_handle_policies_watcher, NULL);*/
  /*workersIterator=&usmWorkers;     // shan't be needed anymore.. even
  basically never did o_o'. while(workersIterator)
      pthread_join(workersIterator->thread_id,NULL);          // some pth_k|c
  there bruh... if (initThreshold) pthread_join(policiesWatcher,NULL);
  // cleaning...
  hashmap_free(usm_alloc_policies);
  hashmap_free(usm_process_linking);
  // if not NULL.. but he/she should've complied...
  //usm->usm_free();
  alloc_usm->usm_free();
  swap_usm->usm_free();*/
}

/*
    Helpers functions to transfer here later... or simply globally define
   them...
*/