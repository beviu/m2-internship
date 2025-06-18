#include "../../include/usm/usm.h" // toMod..

#ifdef DEBUG
bool hsh_iter(const void *item, void *udata) {
  const struct usm_process_link *user = item;
  printf("%s (pol=%ld)\n", user->procName,
         user->alloc_pol); // TODO hmm.. swp_pol...
  return true;
}
int shown = 0;
#endif

/* Add up some other shorter ones..? To get that chee(s|k)y negligible perf.
 * boost huh.. */

int usm_handle_events(struct usm_event *evt) {
  int ret = 0, retried = 0;
  // char * mem;
  struct usm_worker *chosenWorker;
  struct usm_channel *channelNode;
  switch (evt->type) {
  case ALLOC:
#ifdef DEBUG
    if (evt->flags & UFFD_PAGEFAULT_FLAG_WRITE)
      printf("Write attempt : \t");
    else
      printf("Read attempt : \t");
    if (evt->flags & UFFD_PAGEFAULT_FLAG_SHARED)
      printf("(Shared!)\t");
#endif
    if (unlikely(
            evt->flags &
            UFFD_PAGEFAULT_FLAG_SWAP)) { // exec. slow path with thread.. then
                                         // come back after treating other
                                         // events TODO.. | think about manual
                                         // context saving.. #tinyquanta
#ifdef DEBUG
      printf("Sw.A.C\n");
      if (unlikely(!shown)) {
#ifndef NSTOP
        getchar();
#endif
        shown++;
      }
#endif
      ret = 1;
      if (!trySwapCache(evt))
        ret = usmPolicyDefinedSwapIn(evt);
      else {
#ifdef DEBUG
        printf("Resolved through swap cache!\n");
#endif
      }
      // getchar();
    } else {
    retry:
#ifdef DEBUG
      printf("F.A.C..\n");
#endif
      ret = usmPolicyDefinedAlloc(evt);
    }

    // USM tells them we'd need... things.... through evt #SpatialDistrib....,
    // and if he doesn't..... uhhh, later.
    if (unlikely(ret)) {
      if (likely(ret != 1)) {
#ifdef DEBUG
        if (ret == FAULT_EXISTS)
          printf("[Sys] Already allocated..\n");
        else {
          if (ret == FAULT_OOM_VICTIM)
            printf("[Sys] Process be OOM victim..\n");
          else if (ret == 2) // TODO definitions of EDEAD/INVAL styles..     //
                             // started, but dis.. TODO check back 2
            printf(
                "[Sys] Process ciaossu'ed, gracefully ignoring!\n"); // OOMB. to
                                                                     // replace
                                                                     // abortion.
        }/*ret=1;          // ain't here for perf.s, and just making it fail for further debug purposes..
                break;*/          // temp. out
#endif
        break;
      }
      if (unlikely(retried)) {
#ifdef DEBUG
        printf("[Sys] Failed allocation! Aborting. (errno temp. ain't "
               "working.. so nope, grac'ing*)\n"); // OOMB. to replace abortion.
#endif
        ret = 1;
        break; // abort();     // temporary..
      }
#ifdef DEBUG
      printf("[Sys] Failed allocation! Assuming full memory and trying to swap "
             "out once.\n"); // basically full memory, but could be related to a
                             // failed submission... #toMod./Complify
#endif
      usm_evict_fallback(
          evt); // maybe some return value and no bother going back up if
                // nothing done.. but trying again can't hurt, as something
                // could have been freed meanwhile..
      // evict_fallback.. if good, goto ^, if not, OOM... so boom everything for
      // now or just ignore that process for a while... OOM bhvr code.. // gotta
      // be really extremely rare.. -_-'.. with a good swapping policy
      // obviously...
      retried++;
      goto retry;
    }
#ifdef DEBUG
    printf("Memory consumed (unsynched) : %.2f%s\n",
           usedMemory / 1024 / 1024 > 0 ? (float)usedMemory / 1024 / 1024
                                        : (float)usedMemory / 1024,
           usedMemory / 1024 / 1024 > 0 ? "MB"
                                        : "KB"); // should be done by dev.?
#endif
    break;
  case PHY_AS_CHANGE:
#ifdef DEBUG
    printf("Received %lu ! #freePage\n", evt->paddr);
#endif
    ret = usmPolicyDefinedPageIndexFree(evt); // struct page...! 'event.....
                                              // do_cond_swap_in();
#ifdef DEBUG
    printf("Memory consumed : %.2f%s #freePage\n",
           usedMemory / 1024 / 1024 > 0 ? (float)usedMemory / 1024 / 1024
                                        : (float)usedMemory / 1024,
           usedMemory / 1024 / 1024 > 0 ? "MB"
                                        : "KB"); // should be done by dev.?
#endif
    break;
  case FREE:
    // ret = usmPolicyDefinedFree(evt);		// origin'll be compliant..
    break;
  case VIRT_AS_CHANGE:
    // remapping... call swapping code or simply update all relevant fields
    // (e.g. virt. addresses in pages..)
    break;
  case NEW_PROC:
    // assignment strat... +workerChoice..RandifNotDefined(will be treated in
    // chooseWorker)
    usmSetupProcess(evt);
    /*
            for uffd|usm, there's, for the moment, one process per channel....
       quite easily alterable though.
    */
    // chosenWorker = usmChooseWorker();    // we get a new one.. and make it
    // blocking.
    /*appendUSMChannel(&usm_cnl_userfaultfd_ops,evt->origin,&(chosenWorker->usm_channels),&chosenWorker->chnmutex);*/ // inside usmSetupProcess?
    createUSMChannel(&usm_cnl_userfaultfd_ops, evt->origin);
#ifdef DEBUG
    printf("\n\tProcess %d welcomed by a new worker.\n", evt->origin);
#endif
    // We could wake the process here instead...
    break;
#ifndef FBASED
  case NEW_THREAD:
    int regentsID;
    // assignment strat... +workerChoice..RandifNotDefined(will be treated in
    // chooseWorker) usmSetupThread(evt);... eventually o_o'..
    /*
            for uffd|usm, there's, for the moment, one process per channel....
       quite easily alterable though.
    */
    // ye.. usmSetupThread mandatory... -_-'
    // pthread_mutex_lock(&procDescList[evt->origin].tMapLock); // if a thread
    // does a pthread while another does so.. or a fork.. TODO inv.
  newUp:
    evt->usmmem = mmap(
        NULL, SYS_PAGE_SIZE, PROT_READ | PROT_WRITE, /*MAP_FIXED |*/ MAP_SHARED,
        evt->origin,
        0); // usmTMapFd         // evt->usmmem reused but meh, functionally
            // prone to illusions/misguidance, as always overwritten... :')\/
    regentsID = *(int *)(((uintptr_t)evt->usmmem) + 4000);
    // pthread_mutex_unlock(&procDescList[evt->origin].tMapLock);
    if (evt->usmmem == MAP_FAILED) {
      if (errno == ENOMEM) {
        if (nb_vcpus < MAX_VCPUS) {
          struct usm_worker *newWorker =
              (struct usm_worker *)malloc(sizeof(struct usm_worker));
          struct usm_worker *sailor = &usmMemWorkers;
          printf("[Sys] Full troups, conscripting one!\n");
          pthread_mutex_lock(&managersLock);
          newWorker->com = mmap(NULL, SYS_PAGE_SIZE, PROT_READ | PROT_WRITE,
                                /*MAP_FIXED |*/ MAP_SHARED, uffd, 0);
          nb_vcpus++;
          while (sailor->next)
            sailor = sailor->next;
          sailor->next = newWorker;

          // watchList up'ping but legacy for now..

          pthread_mutex_unlock(&managersLock);

          newWorker->id =
              *(int *)((uintptr_t)(newWorker->com) + 3000 + sizeof(int));

          // attrs and such TODO..
          pthread_attr_init(&newWorker->attrs);
          newWorker->usm_wk_ops = malloc(sizeof(struct usm_worker_ops));
          newWorker->usm_wk_ops->usm_handle_evt = usm_handle_events;
          newWorker->thread_name = (char *)malloc(260);
          newWorker->state = (bool *)((uintptr_t)(newWorker->com) + 4092);
          *(newWorker->state) = true; // okay : make this a distant mutex lock..
                                      // might be faster..? TODO TOSUPERDO
          for (int i = 0; i < MAX_UTHREADS; i++) {
            newWorker->usm_channels[i].usm_cnl_id = newWorker->id;
          }
          bzero(newWorker->thread_name, 260);
          sprintf(newWorker->thread_name, "memWorker-%d", newWorker->id);
          sprintf(newWorker->thread_name + strlen(newWorker->thread_name),
                  "/bravo%d", newWorker->id);

          pthread_create(&newWorker->thread_id, NULL /* attrs*/, soldiers,
                         newWorker);
        } else {
          printf("[Sys/Mayday] Max managers and all full, waiting...\n");
          sleep(1);
        }
        goto newUp;
      } else {
#ifdef DEBUG
        perror("[Sys] Thread mmap failed!");
#endif
        ret = 1;
        break;
      }
    }
#ifdef DEBUG
    printf("\n\tBefore writing.. %d, plus dis status.. : %d.\n",
           *(int *)evt->usmmem, *(int *)(evt->usmmem + sizeof(int)));
#endif
    __WRITE_ONCE(*(int *)evt->usmmem,
                 evt->origin); // actually useful as acting buffering between
                               // last task's death and new one's arrival..
    __WRITE_ONCE(
        *(int *)(evt->usmmem + sizeof(int) * 2 + sizeof(unsigned long) * 3),
        0); // or just here o_o'.. -_-'... #duplicate               // TODO
            // investigate actual relevance.. a bit later.           // state..
            // inv. in process.. but :__ver.down..

    chosenWorker = usmChooseWorker(
        regentsID); // potentially creating a new one if full.. TODO | that's
                    // done in kernel space.. so this choosing's now simply
                    // irrelevant.. just take the value chosen in there.. or
                    // simply the last created here temporarily, but yeah,
                    // kernel's gonna scan up to one open, which can be a former
                    // one.. so simply take resulting value there..
    // chosenWorker->usm_channels[]=chosenWorker->id;
    createUSMMemChannel(
        &usm_cnl_mem_ops, evt->usmmem,
        chosenWorker->usm_channels); // appendUSMMemChannel(&usm_cnl_mem_ops,mem,&(chosenWorker->usm_channels),
                                     // &chosenWorker->chnmutex); // inside
                                     // usmSetupThread? #L.
#ifdef DEBUG
    printf("\n\tAfter writing.. %d, plus dis status.. : %d.\n",
           *(int *)evt->usmmem, *(int *)(evt->usmmem + sizeof(int)));
    printf("\n\tThread from process %d welcomed by regent %s with ID %d.\n",
           evt->origin, chosenWorker->thread_name, regentsID);
#ifndef NSTOP
    getchar();
#endif
#endif
    // We could wake the process here instead...            // wut..
    break;
#endif
  case PROC_DTH: /* .. eventually need to hold kernel side ctx.., pure every
                    related event at this' reception, then release it here
                    alongside everything else.. them allocations don't take this
                    all into account until now..*/
    // cleaning.. | Legacy.!
    /*pthread_mutex_lock(evt->chnlock);
    channelNode=list_entry(evt->channelNode, struct usm_channel, iulist);
    list_del(evt->channelNode);
    pthread_mutex_unlock(evt->chnlock);
    // absolutely unsufficient... prev. and next're involved.. freak it man...!
    But... are they problematic... free(channelNode);*/
    // free(channelNode);
    //  no need to check all them page table elements... as they'll be received
    //  either way!
    resetUsmProcess(
        evt->origin); // bound to create some troubles if lock not used....
#ifdef DEBUG
    printf("\n\tProcess %d just reached the afterworld.\n", evt->origin);
#endif
    break;
#ifndef FBASED
  case THREAD_DTH: /* .. eventually need to hold kernel side ctx.., pure every
                      related event at this' reception, then release it here
                      alongside everything else.. them allocations don't take
                      this all into account until now..*/
                   // cleaning..
                   /*pthread_mutex_lock(evt->chnlock);
                   channelNode=list_entry(evt->channelNode, struct usm_channel, iulist);                //
                   temp. name TODO freaking do it right with worker..
                   list_del(evt->channelNode);
                   pthread_mutex_unlock(evt->chnlock);*/

    // munmap(evt->usmmem, SYS_PAGE_SIZE);          // not here man.. not here.

    // free(channelNode);    // lol..
    // free(channelNode);
    //  no need to check all them page table elements... as they'll be received
    //  either way! resetUsmProcess(evt->origin);
    //  // bound to create some troubles if lock not used....
#ifdef DEBUG
    printf("\n\tThread from %d just reached the afterworld. "
           "#uselessAsNotMunmapedHere..\n",
           evt->origin); // TODO take care of dis..
#endif
    break;
#endif
  default:
#ifdef DEBUG
    printf("Weird event.. \n");
#endif
    ret = 1;
    break;
  }
  /*__do_cond_swap_out(evt);              process no tanjobi... ikene
  yo..sonnamon ^^'... matte.. ikeru -_-'... thiotto okashi kedo thiooto dake
  ikeru -_-... TODO.
  __do_cond_swap_in(evt);       */
  // TODO if failed.. no to rest...
  do_cond_swap_in(evt); /* Hence merge the two..? */
  do_cond_swap_out(evt);
  return ret;
}

/*int events_compare(void *priv, const struct list_head *a, const struct
list_head *b) { return
usmGetEventPriority(list_entry(a,usm_events_node,iulist)->event)<usmGetEventPriority(list_entry(b,usm_events_node,iulist)->event)?1:0;
// TODO mod. for compliance.. when we'll actually sort...
}

int usm_sort_events(struct list_head *events) {
    list_sort(NULL,events,events_compare);
}*/