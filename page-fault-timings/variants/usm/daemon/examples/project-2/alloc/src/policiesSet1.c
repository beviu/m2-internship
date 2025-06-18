#include "../incl/policiesSet1.h"

/*
    Basic allocation functions serving as examples. One is required to take a
   lock whe(r|n)ever touching structures, as those are shared between threads.
        Per page locking proposed through -DPLock compilation option, but really
   not recommended. optEludeList can be changed to any type one'd like, but the
   related placeholder's quite needed (or any other method one deems wanted) to
   efficiently mark freed pages as such. The same goes for any related point.
   One has unlimited freedom, just with some logic in linked matters in the API
   we recommend to read.
*/

/*
    pthread_t internalLocks; -> to initialize in defined usm_setup....
*/

/*
    If you really don't want to use the position pointer, or add more in the
   case of multiple levels implied, feel free to modify usm's proposed page
   structure in usm.h. Just try not to heavify it too much.
*/

/*
    One dumb idea : reserving pools through defined functions appointed to
   different policies mapped to processes through the config. file (x set of
   functions treats of one portion of USM's memory and the other ones the rest
   of the latter)...

*/

pthread_mutex_t policiesSet1Ulock;
pthread_mutex_t policiesSet1Flock;

LIST_HEAD(usedList);
LIST_HEAD(freeList);

struct p_args_p {
  int origin, ret /*, nb_unacct*/; /* needs to be taken before giving off com.
                                      zone... sadly.. */
  unsigned long addr;
  struct list_head *l_ps;
};

// struct usm_ops dev_usm_ops;         // just one for now but can be greater,
// i.e. multiple USM instances per arbitrary policies....

void put_page(struct optEludeList
                  *pageNode) { // given list_head should always be sanitized..
  // int count = 1;   // temp. comment, TODO FIX SWP MOD.S side..
  pthread_mutex_lock(&policiesSet1Flock);
#ifdef DEBUG
  printf("Giving back one page..\n");
#endif
  // struct optEludeList * polPage = list_entry(pagesIterator, struct
  // optEludeList, iulist); memset((void*)(polPage->usmPage->data), 0, 4096);
  list_move_tail(&pageNode->iulist,
                 &freeList); // some unneeded list_del.. maybe some verification
                             // after the pages' putting...
  // now acc'ted later usedMemory-=SYS_PAGE_SIZE;                          //
  // some special variable containing "will be used" pages that still aren't..?
  // count--;
  /*#ifdef DEBUG
          printf("Memory consumed : %.2f%s\n",
  usedMemory/1024/1024>0?(float)usedMemory/1024/1024:(float)usedMemory/1024,
  usedMemory/1024/1024>0?"MB":"KB");    // should be done by dev.? #endif*/
  /*if (count){
      //struct optEludeList * polPage = list_entry(pages, struct optEludeList,
  iulist);
      //memset((void*)(polPage->usmPage->data), 0, 4096);
      list_add_tail(pages,&freeList);     // temp..
      usedMemory-=SYS_PAGE_SIZE;
  }*/
  pthread_mutex_unlock(&policiesSet1Flock);
}

void consume_pages(
    struct list_head *pages) { // given list_head should always be sanitized..
  struct list_head *pagesIterator, *tempPageItr;
  pthread_mutex_lock(&policiesSet1Ulock);
  list_for_each_safe(pagesIterator, tempPageItr, pages) {
    // struct optEludeList * polPage = list_entry(pagesIterator, struct
    // optEludeList, iulist);
    list_move_tail(pagesIterator, &usedList);
    usedMemory += SYS_PAGE_SIZE;
  }
  pthread_mutex_unlock(&policiesSet1Ulock);
}

void consume_pages_acct(
    struct list_head *pages, unsigned long addr,
    int origin) { // given list_head should always be sanitized..
  struct optEludeList *node, *node_s;
  list_for_each_entry_safe(node, node_s, pages, iulist) {
    // usmSetUsedListNode(node->usmPage, node); //
    // freeListNode->usmPage->usedListPositionPointer=freeListNode;        //
    // this might be why you did that.. but look, put NULL inside, do the
    // alloc., then in free add some wait time if NULL found.. extreme cases
    // though..
    node->usmPage->virtualAddress = addr;
    node->usmPage->process = origin; // usmLinkPage(chosenPage->usmPage,event);
    pthread_mutex_lock(&policiesSet1Ulock);
    list_move_tail(&node->iulist, &usedList);
    usedMemory += SYS_PAGE_SIZE;
    pthread_mutex_unlock(&policiesSet1Ulock);
    addr += SYS_PAGE_SIZE;
  }
#ifdef DEBUG
  printf("Memory consumed (synched) : %.2f%s\n",
         usedMemory / 1024 / 1024 > 0 ? (float)usedMemory / 1024 / 1024
                                      : (float)usedMemory / 1024,
         usedMemory / 1024 / 1024 > 0 ? "MB" : "KB"); // should be done by dev.?
#endif
}

void *pages_acct(void *p_args) { // no need anymore of "poll" and opposed
                                 // concepts.. they'll be in usm_check..
  int origin = ((struct p_args_p *)p_args)->origin,
      ret = ((struct p_args_p *)p_args)->ret;
  unsigned long addr = ((struct p_args_p *)p_args)->addr;
  struct list_head *l_ps = ((struct p_args_p *)p_args)->l_ps;
  free(p_args);

  if (unlikely(ret)) {
#ifdef DEBUG
    printf("[devPolicies/Mayday] Unapplied allocation(s) (TV)!\n");
    // getchar();
#endif
    put_pages(l_ps);
  } else
    consume_pages_acct(l_ps, addr, origin);
}

/* Returns the remaining pages that couldn't be taken */
int get_pages_prep(
    struct list_head *placeholder, int nr,
    struct usm_event *event) { // that one sizeof adding.. make it arith. point.
                               // thingy.. or at least couple it out.. so that
                               // most get's calculated at compile time..
  int nbr = nr, pos = 0;
  unsigned long vpfn;
  struct optEludeList *chosenPage;
  pthread_mutex_lock(&policiesSet1Flock);
  if (unlikely(list_empty(&freeList))) {
    pthread_mutex_unlock(&policiesSet1Flock);
    return 0;
  }
  chosenPage = list_first_entry(&freeList, struct optEludeList, iulist);
  list_move_tail(&chosenPage->iulist, placeholder);
  // usedMemory+=SYS_PAGE_SIZE;
  pthread_mutex_unlock(&policiesSet1Flock);
  event->paddr = chosenPage->usmPage->physicalAddress;
#ifdef DEBUG
  printf("[devPolicies] Chosen addr.:%lu\n", event->paddr);
#endif

  nr--;
  // LOL!
  // if(nr>0) {
  pthread_mutex_lock(&policiesSet1Flock);
  if (unlikely(list_empty(&freeList))) {
    pthread_mutex_unlock(&policiesSet1Flock);
    return nbr - nr;
  }
  chosenPage = list_first_entry(&freeList, struct optEludeList, iulist);
  list_move_tail(&chosenPage->iulist, placeholder);
  // usedMemory+=SYS_PAGE_SIZE;
  pthread_mutex_unlock(&policiesSet1Flock);
  vpfn = chosenPage->usmPage->physicalAddress;
  usmPrepPreAlloc(event, vpfn++, pos);

  // usmSetUsedListNode(chosenPage->usmPage, chosenPage);  // at consumption
  // trial.. usmLinkPage(chosenPage->usmPage,event);
  // event->vaddr+=SYS_PAGE_SIZE;

  int count = 0;
#ifdef DEBUG
  printf("Range : %ld", vpfn - 1);
#endif
  //}
  while (nr > 0) {
    unsigned long pfn;
    pthread_mutex_lock(&policiesSet1Flock);
    if (unlikely(list_empty(&freeList))) {
      pthread_mutex_unlock(&policiesSet1Flock);
      break;
    }
    chosenPage = list_first_entry(&freeList, struct optEludeList, iulist);
        /* This can be further specialized */ // and man, no need to
                                              // containerOut and containerIn...
                                              // man...
    list_move_tail(&chosenPage->iulist, placeholder);
    // usedMemory+=SYS_PAGE_SIZE;          // some other "held" or "temporary"
    // memory might be cool..
    pthread_mutex_unlock(&policiesSet1Flock);
    pfn = chosenPage->usmPage->physicalAddress;
    // usmSetUsedListNode(chosenPage->usmPage, chosenPage); //
    // freeListNode->usmPage->usedListPositionPointer=freeListNode;        //
    // this might be why you did that.. but look, put NULL inside, do the
    // alloc., then in free add some wait time if NULL found.. extreme cases
    // though.. usmLinkPage(chosenPage->usmPage,event);
    // event->paddr=freeListNode->usmPage->physicalAddress;            // we'd
    // need the last one... buffer adding?
#ifdef DEBUG
    // printf("[devPolicies]
    // %lu\t%d/%d\t",freeListNode->usmPage->physicalAddress, pos+1,
    // wantedNumber-1);
    if (event->paddr == 0)
      getchar();
#endif
    if (likely(vpfn == pfn))
      count++; // usmPrepPreAllocFast(event, pos);
    else {
      usmPrepPreAllocFastCounted(event, pos, count);
      usmPrepPreAlloc(event, pfn, ++pos);
      count = 0;
    }
    // event->vaddr+=SYS_PAGE_SIZE;
    vpfn = pfn + 1;
    // getchar();
    nr--;
  }
#ifdef DEBUG
  printf(" %ld\nCount : %d\n", vpfn - 1, count);
#ifndef NSTOP
  getchar();
#endif
#endif
  if (likely(count))
    usmPrepPreAllocFastCounted(event, pos, count);
  return nbr - nr;
}

static inline int basic_alloc_uniq(struct usm_event *event) {

  // As that part's weird enough as is.. let's emulate it here... them copies..

  /* 100 us */
  /*long j;// long then, now, j;
  int i;
  j=0;
  // BEGIN: wasting cpu time
  for(i=1;i<6500;i++)
  {
      // calculate and sum a whole bunch of cube roots
      j+=pow(i,1/3.0);
  }
  // So the compiler doesn't think the loop is trivial
  j+=1;*/

  /* 1 ms */
  /*long j;// long then, now, j;
  int i;
  j=0;
  // BEGIN: wasting cpu time
  for(i=1;i<65000;i++)
  {
      // calculate and sum a whole bunch of cube roots
      j+=pow(i,1/3.0);
  }
  // So the compiler doesn't think the loop is trivial
  j+=1;*/
  /*unsigned long long var = 0, limit = 750000;

      for(unsigned long long i = 0; i < limit; i++)
      {
              var++;
      }
  if (var)
      var++;*/

  /* 10 ms */
  /*long j;// long then, now, j;
  int i;
  j=0;
  // BEGIN: wasting cpu time
  for(i=1;i<(long)650000000*3*2;i++)
  {
      // calculate and sum a whole bunch of cube roots
      j+=pow(i,1/3.0);
  }
  // So the compiler doesn't think the loop is trivial
  j+=1;*/

  int res = 0;
  pthread_mutex_lock(&policiesSet1Flock);
  if (list_empty(&freeList)) {
    pthread_mutex_unlock(&policiesSet1Flock); // func...
    return 1;
  }
  struct optEludeList *freeListNode =
      list_first_entry(&freeList, struct optEludeList, iulist);
  usedMemory +=
      globalPageSize; // though.. if we just simply let the leveraging of
                      // differently defined page sizes to developers... it
                      // shouldn't be gud.. so we'll just call their functions a
                      // number globalPageSize/SYS_PAGE_SIZE of times from USM..
                      // TODO
  list_del(&(freeListNode->iulist)); // list_move_tail(&(freeListNode->iulist),&usedList);
                                     // // hold before moving... swap works open
                                     // eyes.. -_-' TODO
  pthread_mutex_unlock(&policiesSet1Flock);
  if (freeListNode->usmPage == NULL) {
#ifdef DEBUG
    printf("[devPolicies/Mayday] NULL page..! Aborting.\n");
    getchar();
    exit(1);
#endif
    return 1;
  }
  // memset((void*)((freeListNode->usmPage)->data), 0, 4096);        // don't do
  // dis.. bruh.. usmSetUsedListNode(freeListNode->usmPage, freeListNode); //
  // freeListNode->usmPage->usedListPositionPointer=freeListNode;        // this
  // might be why you did that.. but look, put NULL inside, do the alloc., then
  // in free add some wait time if NULL found.. extreme cases though..
  event->paddr =
      freeListNode->usmPage
          ->physicalAddress; // we'd need the last one... buffer adding?
#ifdef DEBUG
  printf("[devPolicies] Chosen addr.:%lu\n", event->paddr);
  if (event->paddr == 0)
    getchar();
#endif
  // retry:
  event->length = 0;
  if (res = usmSubmitAllocEvent(
          event)) { // will be multiple, i.e. multiplicity proposed by event,
                    // and directly applied by usmSubmitAllocEvent
#ifdef DEBUG
    printf("[devPolicies/Mayday] Unapplied allocation\t%d\n", res);
    // getchar();
#endif
    pthread_mutex_lock(&policiesSet1Flock);
    list_add(&(freeListNode->iulist),
             &freeList); // list_move(&(freeListNode->iulist),&freeList);
    usedMemory -=
        globalPageSize; // though.. if we just simply let the leveraging of
                        // differently defined page sizes to developers... it
                        // shouldn't be gud.. so we'll just call their functions
                        // a number globalPageSize/SYS_PAGE_SIZE of times from
                        // USM.. TODO
    pthread_mutex_unlock(&policiesSet1Flock);
    /*pthread_mutex_lock(&procDescList[event->origin].alcLock);       // per
    node lock man! list_del(&freeListNode->proclist);
    pthread_mutex_unlock(&procDescList[event->origin].alcLock); .. now done
    after.. */
    return res; // or just check it here and return 0.. but more work for pol.
                // dev.. so prol'ly no? event.c ma man.
                // goto retry; // some number of times...
  }
  usmLinkPage(freeListNode->usmPage, event);

  pthread_mutex_lock(&policiesSet1Ulock);
  list_add_tail(&(freeListNode->iulist), &usedList);
  pthread_mutex_unlock(&policiesSet1Ulock);
  /*freeListNode->usmPage->virtualAddress=event->vaddr;
  freeListNode->usmPage->process=event->origin;*/
  // increaseProcessAllocatedMemory(event->origin, globalPageSize);    // man,
  // all these hinder the critical path.. TODO kinda move them..?

  /*pthread_mutex_lock(&procDescList[event->origin].alcLock);       // per node
  lock man!
  list_add(&freeListNode->proclist,&procDescList[event->origin].usedList); //
  HUGE TODO hey, this is outrageous... do what you said and add to_swap in
  event, then a helper to take care of in proc. struct. list, and use the latter
  after basic_alloc.. i.e. in event.c and freaking take care of it there.
  freeListNode->usmPage->processUsedListPointer=&freeListNode->proclist;
  pthread_mutex_unlock(&procDescList[event->origin].alcLock);*/

  // ... if recognized pattern.. multiple submits.. and so on....             +
  // VMAs list and size on the way! -> no needless long checks if applicable!
  return 0;
}

static inline int basic_alloc(struct usm_event *event) {
  /*pthread_mutex_lock(&policiesSet1lock);
  if (list_empty(&freeList)) {
      pthread_mutex_unlock(&policiesSet1lock);       // func...
      return 1;
  }
  struct optEludeList * freeListNode = list_first_entry(&freeList, struct
  optEludeList, iulist); usedMemory+=globalPageSize;         // though.. if we
  just simply let the leveraging of differently defined page sizes to
  developers... it shouldn't be gud.. so we'll just call their functions a
  number globalPageSize/SYS_PAGE_SIZE of times from USM.. TODO
  list_move(&(freeListNode->iulist),&usedList);       // hold before moving...
  swap works open eyes.. -_-' TODO pthread_mutex_unlock(&policiesSet1lock);*/
  // struct list_head pages_list;
  //  LIST_HEAD(pages_list);   thread usage..
  struct list_head *pages_list =
                       (struct list_head *)malloc(sizeof(struct list_head)),
                   *pagesListIterator,
                   *tempPageItr; // u'll only have one fault path per channel..
                                 // don't allocate all the time please.
  int wantedNumber, avPages, ret;
  INIT_LIST_HEAD(pages_list);
  wantedNumber = event->length + 1;
  if (wantedNumber > 512)
    wantedNumber = 512;
  avPages = get_pages_prep(pages_list, wantedNumber, event);
  if (unlikely(avPages == 0)) {
    // printf mayday memory full..
    return 1;
  }
#ifdef DEBUG
  printf("Further possible faults : %d\nPages taken : %d #PreAlloc\n",
         wantedNumber - 1,
         avPages); // these freaking boomings ain't problematic (+-1)..
#ifndef NSTOP
  getchar();
#endif
#endif
  /*if(freeListNode->usmPage == NULL) {
#ifdef DEBUG
      printf("[devPolicies/Mayday] NULL page..! Aborting.\n");
      getchar();
      exit(1);
#endif
      return 1;
  }*/
  /*if(list_empty(&pages_list)) {
#ifdef DEBUG
      printf("[devPolicies/Mayday] NULL page..! Aborting.\n");
      getchar();
      exit(1);
#endif
      return 1;
  }*/
  // memset((void*)((freeListNode->usmPage)->data), 0, 4096);
  /*usmSetUsedListNode(freeListNode->usmPage, freeListNode); //
freeListNode->usmPage->usedListPositionPointer=freeListNode;        // this
might be why you did that.. but look, put NULL inside, do the alloc., then in
free add some wait time if NULL found.. extreme cases though..
  event->paddr=freeListNode->usmPage->physicalAddress;            // we'd need
the last one... buffer adding? #ifdef DEBUG printf("[devPolicies] Chosen
addr.:%lu\n",event->paddr); if (event->paddr==0) getchar(); #endif*/

  // printf("Pos. ending : %d\n", pos);
  // event->channelNode=&pages_list;
  // if(pos)
  event->length =
      avPages -
      1; // used to tell USM that it's about mult. alloc.s...        | TODO
         // backwards compatibility issues.. let's just freaking not use
         // length.. as it takes thingies upper than 1 all the time...
  /*else
      event->length=0;        // freaking abnormal..*/

  // retry:
  ret = usmSubmitAllocEvent(event);
  if (likely(!ret)) {
    if (unlikely(event->flags)) {
      int pos = 0;
#ifdef DEBUG
      printf("%lu unconsummed allocations!\n", event->flags);
      getchar();
#endif
      while (event->flags--)
        put_page(((struct optEludeList *)
                      pagesList[usmGetUnacctPage(event, pos) - basePFN]
                          .usedListPositionPointer)); // TODO helperize..
    }
  }
  if (0 /*avPages>1*/) {
    pthread_t *p_acct = malloc(sizeof(pthread_t));
    struct p_args_p *p_args = malloc(sizeof(struct p_args_p));
    p_args->addr = event->vaddr;
    p_args->l_ps = pages_list;
    p_args->origin = event->origin;
    p_args->ret = ret;
    pthread_create(p_acct, NULL, pages_acct, p_args);
  } else {
    if (unlikely(ret)) {
#ifdef DEBUG
      printf("[devPolicies/Mayday] Unapplied allocation(s)!\n");
      // getchar();
#endif
      put_pages(pages_list);
      return 1;
    } else
      consume_pages_acct(pages_list, event->vaddr, event->origin);
  }

  // here.. do one linkPage (so later) and instead of doing usmLinkPages
  // direclty, check bits field!

  // usmLinkPage(freeListNode->usmPage,event);     // bruh.. no later... 'COS OF
  // CONCURRENCY THINGIES ON FREAKING SWAP MOD.'S SIDE... but still.. hell...
  // why'd you try out some swapping even before applying it.. we just shouldn't
  // play with the devil there.. -_-'.. or not make it "seen" from here too soon
  // enough.. hmm.. yee..

  /*
      For.. decided pages number..
          check bit field, reversely.. if 1, boom out back to freeList (further
     check condition returned maybe..? In like h u m, freaking bad page given..
     but the basic assumption should be the opposite, right..?) if freaking not,
     usmLinkPage.


          So, but, man.. INSTEAD OF PUTTING THEM DIRECTLY TO FREAKING USEDLIST,
     HOLD THEM FIRST, IN SAWP MODULE's MANNER!


          printf("bit %d: %d\n", i, ((c >> i) & 1));
  */

  // consume_pages(&pages_list);

  // put_pages(&pages_list);

  /*freeListNode->usmPage->virtualAddress=event->vaddr;
  freeListNode->usmPage->process=event->origin;*/
  // increaseProcessAllocatedMemory(event->origin, globalPageSize);    // man,
  // all these hinder the critical path.. TODO kinda move them..?

  /*pthread_mutex_lock(&procDescList[event->origin].alcLock);       // per node
  lock man!
  list_add(&freeListNode->proclist,&procDescList[event->origin].usedList); //
  HUGE TODO hey, this is outrageous... do what you said and add to_swap in
  event, then a helper to take care of in proc. struct. list, and use the latter
  after basic_alloc.. i.e. in event.c and freaking take care of it there.
  freeListNode->usmPage->processUsedListPointer=&freeListNode->proclist;
  pthread_mutex_unlock(&procDescList[event->origin].alcLock);*/

  // ... if recognized pattern.. multiple submits.. and so on....             +
  // VMAs list and size on the way! -> no needless long checks if applicable!
  return 0;
}

static inline int basic_c_alloc(struct usm_event *event) {
  if (basic_c_alloc(event))
    return 1;
  while (1) {
    event->vaddr += SYS_PAGE_SIZE;
    printf("Trying again..\n");
    // getchar();
    if (basic_c_alloc(event)) {
      printf("Out!\n");
      break;
    }
  }
  return 0;
}

static inline int double_alloc(struct usm_event *event) {
  if (basic_alloc(event))
    return 1;
  event->vaddr += SYS_PAGE_SIZE; // spatial distrib. file could tell the next
                                 // vaddr to allocate!
  if (basic_alloc(
          event)) { // second one's not obliged to work... i.e. VMA's full..&Co.
                    /*
                    pthread_mutex_lock(&policiesSet1lock);
                    list_move(&((struct optEludeList
                    *)usmPfnToUsedListNode(usmPfnToPageIndex(event->paddr)))->iulist,&freeList);
                    //list_move(&((struct optEludeList
                    *)usmPageToUsedListNode(usmEventToPage(event)))->iulist,&freeList);
                    pthread_mutex_unlock(&policiesSet1lock);
                    usmResetUsedListNode(usmEventToPage(event));
                    //pagesList[usmPfnToPageIndex(event->paddr)].usedListPositionPointer=NULL;
                
                    ..doesn't make sense..
                    */
#ifdef DEBUG
    printf("[devPolicies/Sys] Unapplied allocation.. though not mandatory.\n");
    getchar();
#endif
  }
  return 0;
}

/*static inline struct page * pair_pages_alloc() {
    pthread_mutex_lock(&policiesSet1lock);
    if (list_empty(&freeList)){
        pthread_mutex_unlock(&policiesSet1lock);
        return NULL;
    }
    struct optEludeList * freeListNode = list_first_entry(&freeList, struct
optEludeList, iulist); while ((freeListNode->usmPage)->physicalAddress%2!=0) {
        if (list_empty(&(freeListNode)->iulist)){
            pthread_mutex_unlock(&policiesSet1lock);
            return NULL;
        }
        freeListNode=list_first_entry(&(freeListNode)->iulist, struct
optEludeList, iulist);
    }
    usedMemory+=SYS_PAGE_SIZE;
    freeListNode->usmPage->usedListPositionPointer=freeListNode;    //
toMove.... list_move(&(freeListNode->iulist),&usedList);
    pthread_mutex_unlock(&policiesSet1lock);

    //ret->virtualAddress=va;

    return freeListNode->usmPage;
}*/

int reverse_alloc(struct usm_event *event) {
  int ret = 0;
#ifdef DEBUG
  printf("[devPolicies]Reversing!\n");
  // getchar();
#endif
  pthread_mutex_lock(&policiesSet1Flock);
  if (list_empty(&freeList)) {
    pthread_mutex_unlock(&policiesSet1Flock);
    return 1;
  }
  struct optEludeList *freeListNode =
      list_entry(freeList.prev, struct optEludeList, iulist);
  usedMemory += globalPageSize;
  freeListNode->usmPage->usedListPositionPointer = freeListNode;
  list_move_tail(&(freeListNode->iulist),
                 &usedList); // obsolete with new locks.. riddance needed since
                             // a moment anwyway..
  pthread_mutex_unlock(&policiesSet1Flock);
  event->paddr = freeListNode->usmPage->physicalAddress;
#ifdef DEBUG
  printf("[devPolicies]Chosen addr.:%lu\n", event->paddr);
  if (event->paddr == 0)
    getchar();
#endif
  event->length = 0; // .. but not used atm... man..
  ret = usmSubmitAllocEvent(event);
  if (ret) {
#ifdef DEBUG
    printf("[devPolicies/Mayday] Unapplied allocation..\n");
    getchar();
#endif
    pthread_mutex_lock(&policiesSet1Flock);
    list_move(&(freeListNode->iulist), &freeList); // same here (obsolete)..
    usedMemory -= globalPageSize;
    pthread_mutex_unlock(&policiesSet1Flock);
    return ret; // TODO some other value in order not to assume full memory..
  }
  increaseProcessAllocatedMemory(event->origin, globalPageSize);
  freeListNode->usmPage->virtualAddress = event->vaddr;
  freeListNode->usmPage->process = event->origin;
  return ret;
}

static inline int basic_free(struct usm_event *event) {
  int ret = 0;
  struct optEludeList *tempEntry;
  list_for_each_entry(tempEntry, &usedList, iulist) { // per process.... DI.
    if (tempEntry->usmPage->virtualAddress == event->vaddr)
      break;
  }
  if (&tempEntry->iulist == &usedList) {
    printf("[devPolicies/Mayday] Corresponding page not found!\n");
    ret = 1;
    goto out;
  }
  pthread_mutex_lock(&policiesSet1Flock);
  list_move(&tempEntry->iulist, &freeList); // same here yet again..
  usedMemory -= SYS_PAGE_SIZE;
  pthread_mutex_unlock(&policiesSet1Flock);
  // list_del and free of per proc usedList.. (basic_free's essentially not
  // used.., so later..)
  memset((void *)((tempEntry->usmPage)->data), 0, 4096);
#ifdef DEBUG
  printf("\t%lu | %lu\n", event->vaddr,
         ((struct page *)(tempEntry->usmPage))->physicalAddress);
#endif
  tempEntry->usmPage->virtualAddress = 0;
  decreaseProcessAllocatedMemory(tempEntry->usmPage->process, SYS_PAGE_SIZE);
  tempEntry->usmPage->process = 0;
  tempEntry->usmPage->usedListPositionPointer = NULL;
out:
  return ret;
}

static inline int pindex_free(struct usm_event *event) {
  struct page *usmPage = usmEventToPage(event);
  if (unlikely(!usmPage)) {
#ifdef DEBUG
    printf("[devPolicies/Mayday] Event corresponding to no page\n");
#endif
    return 1;
  }
  if (unlikely(!usmPageToUsedListNode(usmPage))) {
    printf("[devPolicies/Sys] Calling basic free!\n");
    getchar();
    event->vaddr = usmPage->virtualAddress;
    return basic_free(event);
  }

  /*pthread_mutex_lock(&procDescList[usmPage->process].alcLock);
  list_del_init(usmPage->processUsedListPointer);                         //
  example of a simplification of need of use of usmPageToUsedListNode..
  pthread_mutex_unlock(&procDescList[usmPage->process].alcLock);*/

  memset((void *)(usmPage->data), 0,
         4096); // many things to do.. with again new locks..
#ifdef DEBUG
  printf("\t[devPolicies]Collecting freed %lu\n", usmPage->physicalAddress);
#endif
  pthread_mutex_lock(&policiesSet1Flock);
  list_move_tail(
      &((struct optEludeList *)usmPageToUsedListNode(usmPage))->iulist,
      &freeList);
  usmPage->virtualAddress = 0;
  // decreaseProcessAllocatedMemory(usmPage->process, SYS_PAGE_SIZE);     // can
  // and will always only be of SYS_PAGE_SIZE...
  usmPage->process = 0;
  usedMemory -= SYS_PAGE_SIZE;
  pthread_mutex_unlock(&policiesSet1Flock);
  return 0;
}

/* Returns the remaining pages that couldn't be taken */
int pick_pages(struct list_head *placeholder, int nr) {
  int nbr = nr;
  pthread_mutex_lock(&policiesSet1Flock);
  while (nr > 0) {
    if (unlikely(list_empty(&freeList)))
      break;
    struct optEludeList *chosenPage =
        list_first_entry(&freeList, struct optEludeList, iulist);
        /* This can be further specialized */ // and man, no need to
                                              // containerOut and containerIn...
                                              // man...
    list_move(&chosenPage->iulist, placeholder);
    usedMemory += SYS_PAGE_SIZE; // some other "held" or "temporary" memory
                                 // might be cool..
    nr--;
  }
  pthread_mutex_unlock(&policiesSet1Flock);
  return nbr - nr;
}

void give_back_pages(
    struct list_head *pages) { // given list_head should always be sanitized..
  struct list_head *pagesIterator, *tempPageItr;
  // int count = 1;   // temp. comment, TODO FIX SWP MOD.S side..
  pthread_mutex_lock(&policiesSet1Flock);
  /*list_move_tail(pages,&freeList);                 // some unneeded list_del..
  maybe some verification after the pages' putting...
  usedMemory-=SYS_PAGE_SIZE;*/
  list_for_each_safe(pagesIterator, tempPageItr, pages) {
#ifdef DEBUG
    printf("Giving back one page..\n");
#endif
    // struct optEludeList * polPage = list_entry(pagesIterator, struct
    // optEludeList, iulist); memset((void*)(polPage->usmPage->data), 0, 4096);
    list_move_tail(pagesIterator,
                   &freeList);   // some unneeded list_del.. maybe some
                                 // verification after the pages' putting...
    usedMemory -= SYS_PAGE_SIZE; // some special variable containing "will be
                                 // used" pages that still aren't..?
                                 // count--;
#ifdef DEBUG
    printf("Memory consumed : %.2f%s #swapPage(&tempPreAlloc)\n",
           usedMemory / 1024 / 1024 > 0 ? (float)usedMemory / 1024 / 1024
                                        : (float)usedMemory / 1024,
           usedMemory / 1024 / 1024 > 0 ? "MB"
                                        : "KB"); // should be done by dev.?
#endif
  }
  /*if (count){
      //struct optEludeList * polPage = list_entry(pages, struct optEludeList,
  iulist);
      //memset((void*)(polPage->usmPage->data), 0, 4096);
      list_add_tail(pages,&freeList);     // temp..
      usedMemory-=SYS_PAGE_SIZE;
  }*/
  pthread_mutex_unlock(&policiesSet1Flock);
}

void give_back_page_used(struct list_head *page) { // (plural) too version maybe
  pthread_mutex_lock(&policiesSet1Ulock);
  list_add(page, &usedList);
  pthread_mutex_unlock(&policiesSet1Ulock);
}

void hold_used_page_commit(struct list_head *page) {
  pthread_mutex_lock(&policiesSet1Ulock);
  list_del_init(page); // _init probably not needed.. | not so sure anymore...
                       // deffo needed!
  pthread_mutex_unlock(&policiesSet1Ulock);
}

struct usm_alloc_policy_ops usm_basic_alloc_policy = {.usm_alloc = basic_alloc,
                                                      .usm_pindex_free =
                                                          pindex_free,
                                                      .usm_free = basic_free};
struct usm_alloc_policy_ops alloc_policy_one = {
    .usm_alloc = basic_alloc_uniq /*reverse_alloc*/,
    .usm_pindex_free = pindex_free,
    .usm_free = basic_free};
struct usm_alloc_policy_ops alloc_policy_double = {.usm_alloc = double_alloc,
                                                   .usm_pindex_free =
                                                       pindex_free,
                                                   .usm_free = basic_free};

int policiesSet1_setup(unsigned int pagesNumber) { // alloc.* setup..
  for (int i = 0; i < pagesNumber; i++) {
    struct optEludeList *freeListNode =
        (struct optEludeList *)malloc(sizeof(struct optEludeList));
    freeListNode->usmPage = pagesList + i;               // param.
    pagesList[i].usedListPositionPointer = freeListNode; // why only now.. -_-'
    // pagesList[i].processUsedListPointer... for swapping purposes later..?
    // Dang it.
    INIT_LIST_HEAD(&(freeListNode->iulist));
    list_add_tail(&(freeListNode->iulist), &freeList);
  }
  if (usm_register_alloc_policy(&alloc_policy_one, "policyOne", true))
    return 1;
  if (usm_register_alloc_policy(
          &usm_basic_alloc_policy, "basicPolicy",
          false)) // true basic...    | or just link littleTest/Christine to
                  // policyOne... Idk..
    return 1;
  if (usm_register_alloc_policy(&alloc_policy_double, "doublePolicy", false))
    return 1;
  pthread_mutex_init(&policiesSet1Flock, NULL);
  pthread_mutex_init(&policiesSet1Ulock, NULL);
  get_pages = &pick_pages;
  put_pages = &give_back_pages; // retake/redeem/reclaim(nahh..)_'Pages
  restore_used_page = &give_back_page_used;
  hold_used_page = &hold_used_page_commit;
  // pthread_create... of any policy/locally defined thread doing anything upon
  // live stat.s proposed by USM
  return 0;
}

void pol_structs_free() {
  struct list_head *listIterator, *tempLstIterator;
  list_for_each_safe(listIterator, tempLstIterator, &(freeList)) {
    struct optEludeList *listNode =
        list_entry(listIterator, struct optEludeList, iulist);
    list_del(listIterator);
    free(listNode->usmPage);
    free(listNode);
  }
  list_for_each_safe(listIterator, tempLstIterator, &(usedList)) {
    struct optEludeList *listNode =
        list_entry(listIterator, struct optEludeList, iulist);
    list_del(listIterator);
    free(listNode->usmPage);
    free(listNode);
  }
  // pthread_join.. of the aforedefined ones.
}

struct usm_ops dev_usm_ops = {
  usm_setup : policiesSet1_setup,
  usm_free : pol_structs_free
};
