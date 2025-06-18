// #include "include/com/com.h"
#include "../../include/usm/usm.h"
#include <poll.h>

pthread_mutex_t managersLock = PTHREAD_MUTEX_INITIALIZER;

/*void *usm_handle_evt_poll(void* wk_args) {                              // no
need anymore of "poll" and opposed concepts.. they'll be in usm_check.. struct
usm_worker *wk = (struct usm_worker*) wk_args; struct list_head  *tempChnItr,
*eventsListIterator, *tempEvtItr; struct usm_channel *channelsListIterator;
    struct usm_event *event;

    sigset_t set, old;
        sigemptyset(&set);
        sigaddset(&set, SIGUSR1);
        sigprocmask(SIG_UNBLOCK, &set, &old);
#ifdef DEBUG
        printf("A UGThread's sig. rec.'s been set!\n");
#endif        // owkay! This gotta be global.. but man, triggered even before or
wut.. #ifdef DEBUG printf("A UGThread passing by! \t%s\n", wk->thread_name);
#endif
    /*if(list_empty(&(wk->usm_channels)))
        printf("Yah, empty.., %p\n", &(wk->usm_channels));
    else {
        printf("MAN....%p\n", &(wk->usm_channels));
        list_for_each_entry(channelsListIterator, &(wk->usm_channels), iulist) {
            printf("One!\n");
            printf("Is %d!\n",
channelsListIterator->usm_cnl_type==FD_BASED?channelsListIterator->usm_cnl_ops->usm_check(channelsListIterator->fd,0):channelsListIterator->usm_cnl_ops->usm_check_mem(channelsListIterator->buff,0));
            //getchar();
        }
    }
    goto lol;*//*
    //while(1) {
        list_for_each_entry(channelsListIterator, &(wk->usm_channels), iulist) {
#ifdef DEBUG
            printf("Yo'p!\t\t%s\n", wk->thread_name);
#endif
            int notfn=channelsListIterator->usm_cnl_type==FD_BASED?channelsListIterator->usm_cnl_ops->usm_check(channelsListIterator->fd,0):channelsListIterator->usm_cnl_ops->usm_check_mem(channelsListIterator->buff,0);     // highly inefficient..
            if(notfn) {
#ifdef DEBUG
                printf("Event up!\t\t%s\n", wk->thread_name);
#endif          
                //printf("Damn it.. notfn:%d\n", notfn);
//ralloc:
                event=&(channelsListIterator->event);//malloc(sizeof(struct usm_event));
/*                if (!event) {
#ifdef DEBUG
                    printf("Failed malloc in usm_handle_evt_poll\n");
                    getchar();
#endif
                    goto ralloc;
                }*//*
                if (channelsListIterator->usm_cnl_type==FD_BASED)
                    event->origin=channelsListIterator->fd;
                else
                    event->usmmem=channelsListIterator->buff;
                if(likely(notfn>0)) {   
                    if(likely(notfn==POLLPRI))
                        event->type=NEW_THREAD;
                    else
                        if(unlikely(channelsListIterator->usm_cnl_ops->usm_retrieve_evt(event))) {
                            //free(event);
                            continue;
                        }
                } else {
                    if(channelsListIterator->usm_cnl_type!=RAM_BASED)      // temporary...
                        event->type=PROC_DTH;
                    else
                        event->type=THREAD_DTH;
                    event->channelNode=&(channelsListIterator->iulist);
                    event->chnlock=&(wk->chnmutex);
                }
                /*struct usm_events_node *eventListNode=(usm_events_node *)malloc(sizeof(usm_events_node));
                eventListNode->event=event;
                INIT_LIST_HEAD(&(eventListNode->iulist));*//*
                list_add(&(event->iulist),&(wk->usm_current_events));
                //wk->usm_current_events_length++;      // stats purposes maybe but disposable..
            }
        }
        // wk->usm_wk_ops->usm_sort_evts(&wk->usm_channels);
        //goto lol;
        if(list_empty(&wk->usm_current_events)) //(!(wk->usm_current_events_length))
            return NULL; // continue;       temp., to comply with lack of loop.
#ifdef DEBUG
        printf("Events' number:%d\t%s\n",wk->usm_current_events_length, wk->thread_name);
#endif
        list_for_each_safe(eventsListIterator,tempEvtItr,&(wk->usm_current_events)) {
            struct usm_event * eventsIterator=list_entry(eventsListIterator, usm_event, iulist);
            if (likely(!(wk->usm_wk_ops->usm_handle_evt(eventsIterator)))) {              // this worker passing thing.. we should just get rid of them channels here man               // should we "likely"..
                //wk->usm_current_events_length--;
                list_del(eventsListIterator);
                //free(eventsIterator->event);
                //free(eventsIterator);
            } else {
                //raise(SIGINT);
#ifdef DEBUG
                printf("Event not treated... dropping for now!\n");
                getchar();
#endif
                // list_move_tail(eventsListIterator,&(wk->usm_current_events));    | move it.. drop it... or wut?
                //wk->usm_current_events_length--;
                list_del(eventsListIterator);
                //free(eventsIterator->event);
                //free(eventsIterator);
                continue;
            }
#ifdef DEBUG
            printf("\nEvent treated..\n");
#endif
        }
    //}
lol:
    while (1) {;}
}*/

void *
usm_handle_evt_poll(void *wk_args) { // no need anymore of "poll" and opposed
                                     // concepts.. they'll be in usm_check..
  struct usm_worker *wk = (struct usm_worker *)wk_args;
  struct list_head *tempChnItr, *eventsListIterator, *tempEvtItr;
  struct usm_channel *channelsListIterator = &wk->usm_channels[0];
  struct usm_event *event = &channelsListIterator->event;

  event->origin = channelsListIterator->fd;

  printf("Me here!\t\t%s\n", wk->thread_name);

  while (1) {
    int notfn = channelsListIterator->usm_cnl_ops->usm_check(
        channelsListIterator->fd, -1);
    if (notfn) {
#ifdef DEBUG
      printf("Event up!\t\t%s\t%d\n", wk->thread_name, notfn);
#endif
      event = &(channelsListIterator->event);
      // event->usmmem=channelsListIterator->buff;    // new design related?
      // This path ain't it, in any case..
      if (likely(notfn >
                 0)) { // shouldn't have any other state at this point TODO
        if (unlikely(
                channelsListIterator->usm_cnl_ops->usm_retrieve_evt(event))) {
          continue;
        }
      }
      /*struct usm_events_node *eventListNode=(usm_events_node
      *)malloc(sizeof(usm_events_node)); eventListNode->event=event;
      INIT_LIST_HEAD(&(eventListNode->iulist));*/
      // list_add(&(event->iulist),&(wk->usm_current_events));    //
      // deprecated..
      // wk->usm_current_events_length++;      // stats purposes maybe but
      // disposable..
    }
#ifdef DEBUG
    if (unlikely(!notfn)) { // should never happen..
      printf("Man.. them blocking states!\t%s\n", wk->thread_name);
      getchar();
      continue;
    }
#endif
    // events number here.. in the sense that the fd could get all its events
    // taken before getting here.. except thread related ones..!
    if (likely(!(wk->usm_wk_ops->usm_handle_evt(
            event)))) { // this worker passing thing.. we should just get rid of
                        // them channels here man               // should we
                        // "likely"..
                        // wk->usm_current_events_length--;
#ifdef DEBUG
      printf(
          "Event treated... get rid of this -_-'\n"); // list_del(eventsListIterator);
#endif
      ;
      // free(eventsIterator->event);
      // free(eventsIterator);
    } else {
      // raise(SIGINT);
#ifdef DEBUG
      printf("Event not treated... dropping for now!\n");
      getchar();
#endif
      ;
      // list_move_tail(eventsListIterator,&(wk->usm_current_events));    | move
      // it.. drop it... or wut?
      // wk->usm_current_events_length--;
      // list_del(eventsListIterator);
      // free(eventsIterator->event);
      // free(eventsIterator);
    }
#ifdef DEBUG
    printf("\nEvent treated..\n");
#endif
  }
}

int usm_handle_mem(
    struct usm_channel
        *channel) { // filled with unneeded legacy thingies weirdly intertwined.
                    // Overhauling further down.
  // struct usm_channel *channel = (struct usm_channel *)arg;
  struct usm_event *event = &(channel->event);
  int ret = 0;
#ifdef DEBUG
  int id =
      *(unsigned int
            *)(event->usmmem + 4096 -
               sizeof(
                   unsigned int)); // only used for debugging..     | but may be
                                   // used some more.. to tighten things up in
                                   // sched.... but Idk the scale of possible
                                   // losses.. contention or other related..
#endif

  int notfn = channel->usm_cnl_ops->usm_check_mem(event->usmmem, 0);
  if (likely(notfn)) {
#ifdef DEBUG
    printf("[memChan-%d] Event up!\n", id);
#endif
    // event=&(channel->event);
    // event->usmmem=channel->buff;     // done at reception..
    if (likely(notfn > 0)) {
      if (unlikely(channel->usm_cnl_ops->usm_retrieve_evt(event))) {
#ifdef DEBUG
        printf("[memChan-%d] Couldn't get event!\n",
               id); // not thread name but channel -> process'
#endif
        return 1;
      }
    } else { // never gets in since new ver., but, man : not channel->buff but
             // event->usmmem...
      event->type = THREAD_DTH; // unnces. atm
      // Unnecessary.. ain't got more than one now.. legacy

      // event->channelNode=&(channel->iulist);
      // event->chnlock=&(wk->chnmutex);

      // Actually nothing to do.. like, next'd just crash on old val.s... unless
      // I'm missing smthn

      // channel->usm_handle_evt=NULL; // wow there cowboy.. preload 'em all
      // actually..? But mem. needing to be mapped.. this then just a
      // in-place-of mem. checker... | hence TODO temp.
      munmap(event->usmmem, SYS_PAGE_SIZE);
      event->usmmem = NULL;
#ifdef DEBUG
      printf("\t\t[memChan-%d] Unregistered!\n", id);
      getchar();
#endif
      return 2;
    }
    if (likely(!(usm_handle_events(
            event)))) { // N., usm_handle_evt.. specialize, TODO, fd too // this
                        // worker passing thing.. we should just get rid of them
                        // channels here man               // should we
                        // "likely"..
#ifdef DEBUG
      printf("[memChan-%d] Fault treated!\n", id);
#endif
    } else {
      // raise(SIGINT);
#ifdef DEBUG
      printf("[memChan-%d] Event not treated... dropping for now!\n", id);
      getchar();
#endif
      ret = 1;
    }
  } else {
#ifdef DEBUG
    printf("[memChan.-%d] Impoossiblé happened..\n", id);
#endif
    ret = 1;
  }

  return ret;
}

int usm_handle_mem_special(struct usm_event *event) {
  int notfn, ret = 0;
#ifdef DEBUG
  int id = *(
      unsigned int
          *)(event->usmmem + 4096 -
             sizeof(unsigned int)); // only used for debugging..     | could be
                                    // extended to some sched logic tightening..
                                    // as used for nothing else ftm.
#endif

  // special.. so.. although prol'ly temporarily.. no channel notion here (-_-)
  // TODO just put it in supposed usm_event's field
  notfn = (&usm_cnl_special_ops)
              ->usm_check_mem(
                  event->usmmem,
                  0); // then boom it out (int status...) then yeesh yesh...
                      // (though after copying to that usm_event...)

  if (likely(notfn)) {
#ifdef DEBUG
    printf("[memChanSpec.-task%d/%d] Event up!\n", id, event->origin);
#endif
    // event=&(channel->event);
    // event->usmmem=channel->buff;     // done at reception..
    if (likely(notfn > 0)) {
      // event->type=ALLOC;           // LtAdgs
      if (unlikely(*(unsigned int *)((uintptr_t)(event->usmmem) +
                                     SYS_PAGE_SIZE -
                                     sizeof(unsigned int) * 3) == 100)) {
        if (unlikely((&usm_cnl_mem_ops)->usm_retrieve_evt(event))) {
#ifdef DEBUG
          printf("[memChanSpec.-task%d] Couldn't get event!\n",
                 id); // not thread name but channel -> process'
#endif
          return 1;
        }
      } else if (unlikely((&usm_cnl_special_ops)->usm_retrieve_evt(event))) {
#ifdef DEBUG
        printf("[memChanSpec-task%d] Couldn't get event!\n",
               id); // not thread name but channel -> process'
#endif
        return 1;
      }
    } else {
      event->type =
          THREAD_DTH; // unnces. atm      // IUTHREAD death here.. o_o' TODO
      /* Unnecessary.. ain't got more than one now.. legacy*/
      // event->channelNode=&(channel->iulist);
      // event->chnlock=&(wk->chnmutex);

      /* Actually nothing to do.. like, next'd just crash on old val.s... unless
       * I'm missing smthn*/
      // channel->usm_handle_evt=NULL; // wow there cowboy.. preload 'em all
      // actually..? But mem. needing to be mapped.. this then just a
      // in-place-of mem. checker... | hence TODO temp.
      munmap(event->usmmem, SYS_PAGE_SIZE);
      // event->usmmem=NULL;     // unnec.    | even more now that they all
      // belong to a special group sharing this..      .. but yeah, this all
      // ain't even got any counterpart (iuthread) whatsoever.. but leaving it
      // here for future work or retro-compat.
#ifdef DEBUG
      printf("[memChanSpec.-task%d] Unregistered!\n", id);
#endif
      return 2;
    }
    if (likely(!(usm_handle_events(
            event)))) { // N., usm_handle_evt.. specialize, TODO, fd too // this
                        // worker passing thing.. we should just get rid of them
                        // channels here man               // should we
                        // "likely"..
#ifdef DEBUG
      printf("[memChanSpec.-task%d] Fault treated!\n", id);
#endif
    } else {
      // raise(SIGINT);
#ifdef DEBUG
      printf("[memChanSpec.-task%d] Event not treated... dropping for now!\n",
             id);
      getchar();
#endif
      ret = 1;
    }
  } else {
    /*if(unlikely(!notfn)) {        // preemptively dropped.. but this TODO fix
kernel level..  ... like he possibly have faulted while we were moving it up, so
we need this..     | nah, we make sure to queue smthn if there was a fault, at
mmap... #ifdef DEBUG printf("[memCHan-%d] Wel'p, temporary internal soldier
attributions' state...\n", id); #endif return 0;
    }*/
#ifdef DEBUG
    printf("[memChanSpec.-task%d] Impoossiblé happened..\n", id);
#endif
    ret = 1;
  }

  return ret;
}

/*int usm_handle_fd(void *arg) {        // doubt we'll use this.. just leave
them fd based thingies in other independant and blockable threads..? struct
usm_channel *channel = (struct usm_channel *)arg; struct usm_event *event; int
ret = 0;

    int notfn=channel->usm_cnl_ops->usm_check(channel->fd,0);
    if(notfn) {
#ifdef DEBUG
        printf("Event up!\t\t%s\n", wk->thread_name);
#endif
        event=&(channel->event);
        event->origin=channel->fd;
        if(likely(notfn>0)) {
            if(unlikely(channel->usm_cnl_ops->usm_retrieve_evt(event))) {
#ifdef DEBUG
                printf("Couldn't get event!\t\t%s\n", wk->thread_name);     //
not thread name but channel -> process' #endif return 1;
            }
        }
        else {
            event->type=PROC_DTH;
            /* Unnecessary.. ain't got more than one now.. legacy*//*
            // event->channelNode=&(channel->iulist);
            // event->chnlock=&(wk->chnmutex);
        }
        if (likely(!(usm_handle_evt(event)))) {              // this worker passing thing.. we should just get rid of them channels here man               // should we "likely"..
#ifdef DEBUG
            printf("Fault or proc death treated!\n");
#endif
        } else {
            //raise(SIGINT);
#ifdef DEBUG
            printf("Event not treated... dropping for now!\n");
            getchar();
#endif
            ret = 1;
        }
    }

    return ret;
}*/

void *usm_handle_fd(
    void *arg) { // doubt we'll use this.. just leave them fd based thingies in
                 // other independant and blockable threads..?
  struct usm_channel *channel = (struct usm_channel *)arg;
  struct usm_event *event;
  int id = channel->fd;

  event = &(channel->event);
  event->origin = channel->fd;
  event->type = NEW_THREAD;
  goto initMainThread;
  while (1) {
    int notfn = channel->usm_cnl_ops->usm_check(channel->fd, -1);
    if (notfn) {
#ifdef DEBUG
      printf("Event up!\t\tid : %d [singular fd majordomos], notfn (%d)\n", id,
             notfn);
#endif
      if (likely(notfn > 0)) {
        if (likely(notfn == POLLPRI))
          event->type = NEW_THREAD;
        else if (unlikely(channel->usm_cnl_ops->usm_retrieve_evt(event))) {
#ifdef DEBUG
          printf("Couldn't get event!\t\tid : %d\n",
                 id); // not thread name but channel -> process'
#endif
          return NULL;
        }
      } else {
        event->type = PROC_DTH;
        /* Unnecessary.. ain't got more than one now.. legacy*/
        // event->channelNode=&(channel->iulist);
        // event->chnlock=&(wk->chnmutex);

        /* Highly sufficient.. just the cleanups of related structs by fd val.
         * TODO */
#ifdef DEBUG
        printf("[singMajordomo-%d] Goodbye!\n", event->origin);
        getchar();
#endif
        // free(&pthread_self())    ?
        procDescList[event->origin].prio =
            -1; // needs comment out of kernel poll code (file closure) to
                // properly work.. but then inv. max fd number.. and see at
                // which point to close it.. maybe after purging 'em out with a
                // thread which'll form it out then link it back.. but then
                // contention with pindex_free! Maybe rid the latter o_O'..
                // philosophically right (!) TODO   tho. closing an anon.
                // thingy, haha!
        free(channel);
        return NULL;
      }
    initMainThread:
      if (likely(!(usm_handle_events(
              event)))) { // this worker passing thing.. we should just get rid
                          // of them channels here man               // should
                          // we "likely"..
#ifdef DEBUG
        if (event->type == NEW_THREAD)
          printf("[singMajordomo-%d] Birth treated!\n", id);
        else {
          if (event->type == THREAD_DTH)
            printf("[singMajordomo-%d] Thread's ciaossu treated!\n", id);
          else if (event->type == PROC_DTH) { // meh should never get dis..
            printf("[singMajordomo-%d] Reverence treated!\n", id);
            getchar();
            return NULL;
          } else
            printf("[singMajordomo-%d] UFFD classic event treated!\n", id);
        }
#endif
      } else {
        // raise(SIGINT);
#ifdef DEBUG
        if (event->type == NEW_THREAD)
          printf("[singMajordomo-%d] Birth not treated..\n", id);
        else {
          if (event->type == THREAD_DTH)
            printf("[singMajordomo-%d] Thread's ciaossu not treated..\n", id);
          else if (event->type == PROC_DTH)
            printf("[singMajordomo-%d] Reverence not treated..\n", id);
          else
            printf("[singMajordomo-%d] UFFD classic event not treated..\n", id);
        }
        printf("[singMajordomo-%d] . stagger'*ing for now!\n", id);
        getchar();
#endif
        return NULL;
      }
    }
#ifdef DEBUG
    else {
      printf("[singMajordomo-%d] Impoossiblé happened..\n", event->origin);
      getchar();
    }
#endif
  }
}

#ifdef UINTR
void __attribute__((interrupt))
__attribute__((target("general-regs-only", "inline-all-stringops")))
scheduler_handlerr(struct __uintr_frame *ui_frame, unsigned long long vector) {
  (void)ui_frame;
  (void)vector;
#else
void scheduler_handlerr(int sig) {
  (void)sig;
#endif
#ifdef DEBUG
  printf("Signal received!\n");
#endif
}

void *usm_mem_worker_handler(void *wk_args) {
  struct usm_worker *wk =
      &usmMemWorkers; // (while i next until...) // (struct usm_worker*)
                      // wk_args; ([IMP] uthreads' bug fix attempt...)
  struct usm_channel *channelsList = wk->usm_channels;
  // struct usm_event *event;    // lotta precharging TODO beforehand.. and not
  // here, as this treats of a lotta 'em.

  int rounds = 0;

  /*sigset_t set, old;
      sigemptyset(&set);
      sigaddset(&set, SIGUSR1);
      sigprocmask(SIG_UNBLOCK, &set, &old);*/

  struct sigaction sa;
  sa.sa_flags = 0;
  sa.sa_sigaction = (void *)scheduler_handlerr;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGUSR1);
  sigaction(SIGUSR1, &sa, NULL);

  *wk->com = 0; // unsigned long long for max but....

  // createUSMMemChannel(&usm_cnl_mem_ops,NULL,channelsList);

  // wk->com = mmap(NULL, SYS_PAGE_SIZE, PROT_READ|PROT_WRITE, /*MAP_FIXED |*/
  // MAP_SHARED, uffd  /* doesn't matter which for now.. */, 0);    // def.fo
  // not here.
nground: // temp., inside loop.. just for momentary style* purposes..
#ifdef DEBUG
  printf("[%s] New round!\n", wk->thread_name);
#endif
  while (1) {
    // here
    unsigned int tempEvents =
        *wk->com; // unsigned long long for max but...     //
                  // unnecessary*(conserving for now for the next copy
                  // needed..).. TODO | junction..
    int ret;
    // and here
    if (!tempEvents) { // __builtin_popcountll(tempEvents)
#ifdef DEBUG
      // printf("[%s] Naught to do..\n", wk->thread_name);
      ;
#endif
      if (rounds == 110) {
#ifdef DEBUG
        printf("[%s] zZz..\n", wk->thread_name);
#endif
        rounds = 0;
        *(wk->state) = false;
        pause(); // return NULL; // swap_context.. for uthread's ver.
        *(wk->state) = true; // duplicata.. o_o'
        goto nground;
      }
      rounds++;
    } else {
      int id = ffs(tempEvents) -
               1; // __lg(tempEvents);      // or just get that -1 before this
                  // all.. meh, should be the same.. -_-'.. let's print JIC..
#ifdef DEBUG
      printf("[%s] Popping ID : %d, %u\n", wk->thread_name, id, *wk->com);
#endif
      __WRITE_ONCE(
          *wk->com,
          *wk->com & (~((unsigned int)1
                        << id))); //*(unsigned long long *)wk->com&=~((unsigned
                                  //long long)1 << id);    // toggle it (ID) off
                                  //(.. leave it) here or there..?
#ifdef DEBUG
      printf("[%s] ID popped : %d, %u\n", wk->thread_name, id, *wk->com);
#endif
      rounds = 0;
      while (unlikely(!channelsList[id].event.usmmem /*buff usm_handle_evt*/)) {
#ifdef DEBUG
        printf(
            "[%s] Halt! memChan-%d not yet registered!\tSHOULD NEVER HAPPEN!\n",
            wk->thread_name, id);
        getchar();
#else
        ;
#endif
      }
#ifdef DEBUG
      printf("[%s] Treating mem. event %d\n", wk->thread_name, id);
#endif
      ret = channelsList[id].usm_handle_evt(
          &(channelsList[id])); // usm_handle_mem(&(channelsList[id]));      //
                                // ret still not needed..      | tinyquanta
      // pop it here.. if ret blablabla..     |   popped before to make sure new
      // faults get switched right in the bit field. | Alain's suggestion.
    }
  }
}

#ifdef UINTR
void __attribute__((interrupt))
__attribute__((target("general-regs-only", "inline-all-stringops")))
waker_handler(struct __uintr_frame *ui_frame, unsigned long long vector) {
  (void)ui_frame;
  (void)vector;
#else
void waker_handler(int sig) {
  (void)sig;
#endif
#ifdef DEBUG
  // vcpu_t *cur_vcpu = &vcpu_list[get_vcpu_id()];
  printf("[Handling/Waked] %s awoken..!\n",
         "localWaker"); // cur_vcpu->wk_args->thread_name);
#endif
}

void *usm_mem_workers_waker(void *wk_args) {
  struct usm_watch_list *usmWatchList = (struct usm_watch_list *)wk_args;

  struct sigaction sag;
  sag.sa_flags = 0;
  sag.sa_sigaction = (void *)waker_handler;
  sigemptyset(&sag.sa_mask);
  sigaddset(&sag.sa_mask, SIGCHLD);
  sigaction(SIGCHLD, &sag, NULL);

  /*      TODO at usage...
  if (uintr_register_sender(vcpu_list[i].upid_fd, 0) < 0)
              exit(1);
  */

  while (1) {
    struct usm_watch_list *tempList = usmWatchList;
    int i = 0;
    while (tempList) {
      i++;
      if (*tempList->state)
        goto next;
      if (*tempList->com) {
#ifdef UINTR
        // _senduipi(i-1);
        ;
#else
        pthread_kill(*tempList->thread_id, SIGUSR1);
#endif
        __WRITE_ONCE(*tempList->state, 1);
#ifdef DEBUG
        printf("[maydayOne] Triggered vCPU** bravoOne!\n"); // ofc, some %s on
                                                            // bravoOne..
#endif
      }
    next:
      tempList = tempList->next;
    }
    pause();
    // sleep(1);
  }
}

/*usm_handle_evt_periodic(struct usm_worker* wk){           //.. this type of
worker should be specialized/special enough to be launched scarcily ; one
example'd be the thresholds' policies applier... int budget=wk->usm_budget;
    while(1){
again:
        // À implémenter: parcourir wk->usm_channels et placer les événements
dans wk->usm_current_events budget--; if(budget!=0) goto again;
        wk->usm_wk_ops->usm_sort_evts();
        for(int i=0;i<wk->usm_current_events_length;i++){//on traite les
evenements wk->usm_wk_ops->usm_handle_evt(wk->usm_current_events[i]);
        }
        wk->usm_current_events_length=0;
        sleep(5);
    }
}*/                                                         // what could be interesting would be a ring buffer... with a max. number in the loop collecting the events upon which we'd break....

int usm_uffd_ret_ev(struct usm_event *event) {
  struct uffd_msg msg;
#ifdef DEBUG
  printf("Retrieving UFFD event\n");
#endif
  int readres = read(event->origin, &msg, sizeof(msg));
  if (readres == -1) {
#ifdef DEBUG
    printf("[Mayday] Failed to get event\n");
    getchar();
#endif
    return errno;
  }
  // event->process_pid, event->origin to comm_fd, once channels loosened from
  // processes
  switch (msg.event) { // dyent..
  case UFFD_EVENT_PAGEFAULT:
    event->type = ALLOC;
    event->vaddr = msg.arg.pagefault.address;
    // event->length=globalPageSize;        | obvious in the case of a page
    // fault... the size of it'd be gPS, but it's accessible everywhere, so... ;
    // but this should be needed/used later on. | so we now use length for
    // something else.. although related..
    event->flags = msg.arg.pagefault.flags;
    // if unlikely...
    event->offst = msg.arg.pagefault.entry_content * SYS_PAGE_SIZE;
    break;
  case UFFD_EVENT_FORK:
    event->type = NEW_PROC;
    event->origin = msg.arg.fork.ufd;
    break;
  case UFFD_EVENT_REMAP:
    event->type = VIRT_AS_CHANGE;
    // old..new...addrs
    break;
  case UFFD_EVENT_UNMAP:  // this is not specifically true (uffd's way of
                          // handling remap...).. | toMod.
  case UFFD_EVENT_REMOVE: // possibly done by madvise...
    event->type = FREE;
    event->vaddr = msg.arg.remove.start;
    event->length = msg.arg.remove.end - msg.arg.remove.start;
    break;
  /*case UFFD_EVENT_:     // probably new ones later..
      event->type=__;
      break;*/
  default:
#ifdef DEBUG
    printf("[Mayday] Unspecified event\n");
#endif
    return 1;
  }
#ifdef DEBUG

  printf("[UFFD] %d event\n\ttALLOC:%d NEW_PROC:%d VIRT_AS_CHANGE:%d FREE:%d\n",
         event->type, ALLOC, NEW_PROC, VIRT_AS_CHANGE, FREE);
#endif
  return 0;
}

int usm_uffd_subm_ev(struct usm_event *event) {
  usm_set_pfn(event->origin, event->vaddr, event->paddr, 0);
}

int usm_uffd_subm_ev_iu(struct usm_event *event) {
#ifdef DEBUG
  printf(
      "[spec.] Submitting event.., not resetting at this precise point...\n");
#endif
  usm_set_pfn_iuthread(event->origin, event->vaddr, event->paddr,
                       0); // or call it fast* (TODO, no walk anymore..)
}

int usm_uffd_subm_ev_emul(struct usm_event *event) {
  usm_set_pfn_emul(event->origin, event->vaddr, event->paddr,
                   0); // or call it fast* (TODO, no walk anymore..)
}

int usm_freed_page_ret_ev(struct usm_event *event) {
  unsigned long pfn = 0;
  int readres = read(usmFreePagesFd, &pfn, sizeof(pfn));
  if (readres == -1) {
#ifdef DEBUG
    printf("[Mayday] Failed to get freed page event\n");
#endif
    return errno;
  }
  event->paddr = pfn;
  event->type = PHY_AS_CHANGE; // but free..? Wut the.... | dyend.
  // origin? Retrievable elsewhere too..
  return 0;
}

int usm_new_proc_ret_ev(struct usm_event *event) {
  char procName[16];
  int process = read(usmNewProcFd, &procName, sizeof(procName));
  if (process <= 0) {
#ifdef DEBUG
    printf("[Mayday] Failed to get new process event\n");
#endif
    return errno;
  }
  event->procName = NULL; // some free() or mostly static alloc.. as static
                          // (boundary'.ed) back down there...
#ifdef DEBUG
  printf(
      "Received process' name : %s\n",
      procName); /* TODO : investigate its disappearance in the cas of unnamed
                    tasks (e.g. background saver of Redis).. just wth..*/
#ifndef NSTOP
  getchar();
#endif
#endif
  event->origin = process; // not really "origin" here but pfd.. meh...
  event->type = NEW_PROC;
  if (strlen(procName) != 0) {
    event->procName = malloc(sizeof(procName));
    strcpy(event->procName, procName);
  }
  write(usmNewProcFd, NULL,
        0); // shan't be nec....           TODO tighten., pull out from state
            // only in *_fd(), after properly init'ed here. ...tho., latency
            // wise... TOSEE
  return 0;
}

/*int usm_new_proc_subm_ev(struct usm_event * event, int comm_fd) {

}*/

int usm_poll_check(int channel_id, int timeout) { // fix this man.. TODO
  struct pollfd pollfd[1];
  pollfd[0].fd = channel_id;
  pollfd[0].events = POLLIN | POLLPRI;
  int res = poll(pollfd, 1, timeout);
#ifdef DEBUG
  printf("res: %d\trevents: %d\tcbevents: \t\t%d\t%d\t%d\n", res,
         pollfd[0].revents, POLLPRI, POLLIN, POLLERR);
  getchar();
#endif
  if (res <= 0) // basically undefined..
    return res;
  // if (/*pollfd[0].events == POLLPRI || */pollfd[0].revents==POLLPRI /*||
  // pollfd[0].events==-POLLPRI || pollfd[0].revents==-POLLPRI*/) return
  // POLLPRI;
  // if(pollfd[0].events!=POLLIN && pollfd[0].revents!=POLLIN &&
  // pollfd[0].events!=POLLPRI && pollfd[0].revents!=POLLPRI) return -1;
  if (unlikely(pollfd[0].revents != POLLPRI && pollfd[0].revents != POLLIN))
    return -1;
  return pollfd[0].revents; // POLLIN;
}

int usm_poll_check_np(int channel_id, int timeout) { // fix this man.. TODO
  struct pollfd pollfd[1];
  pollfd[0].fd = channel_id;
  pollfd[0].events = POLLIN;
  /*int res=*/poll(pollfd, 1, timeout);
  // printf("Waken! #nPt\n");
  return POLLIN;
}

/*struct usm_ctx {      temporary kernel structure..
        unsigned int status;			// 0 0 0 0 0 .. >> &
#define BIT(nr)			(UL(1) << (nr)).. for a freaking setting up of
bit. |UI unsigned long vaddr;		// and just the size.. manually...
        unsigned long paddr;
};*/

/*  current K. def.s
    int *status = (int *)(current->usm_ctx+sizeof(int));
        unsigned long *vaddr = (unsigned long
   *)(current->usm_ctx+sizeof(int)*2); unsigned long *paddr = (unsigned long
   *)(current->usm_ctx+sizeof(int)*2+sizeof(unsigned long)); unsigned long
   *flags = (unsigned long *)(current->usm_ctx+sizeof(int)*2+sizeof(unsigned
   long)*2);
*/

/*
        | usmfd (int) | status (int) | vaddr (unsigned long) | paddr (unsigned
   long) | flags (unsigned long) | state (int) | rem_size (unsigned long) |
   further_alloc | extra_allocs_plh....reserved for multiple allocations..
   should be really long, so not bothering to get max. for now, but meh, yes,
   later.

        Of course, many ints to be changed to bit ranges! As said in the paper!
   #TODO



    unsigned long *rem_size = (unsigned long
   *)(current->usm_ctx+sizeof(int)*3+sizeof(unsigned long)*3); int
   *further_alloc = (int *)(current->usm_ctx+sizeof(int)*3+sizeof(unsigned
   long)*4); unsigned long *extra_allocs_plh = (unsigned long
   *)(current->usm_ctx+sizeof(int)*4+sizeof(unsigned long)*4);
*/

int usm_mem_ret_ev(struct usm_event *event) {
  unsigned long *vaddr = (unsigned long *)(event->usmmem + 8); // sizeof(int)*2
  unsigned long *paddr =
      (unsigned long *)(event->usmmem +
                        16); // sizeof(int)*2+sizeof(unsigned long)
  unsigned long *flags =
      (unsigned long *)(event->usmmem +
                        24); // sizeof(int)*2+sizeof(unsigned long)*2

  unsigned long *rem_size = (unsigned long *)(event->usmmem + sizeof(int) * 3 +
                                              sizeof(unsigned long) * 3);

#ifdef DEBUG
  printf("Retrieving mem. based fault event\n");
#endif

  // event->origin=__READ_ONCE(*(int*)event->usmmem);     // LtAdgs
  // printf("Origin : %d\n", event->origin);
  // tempmem+=(sizeof(int)*2);       // can be done before putting in the event
  // or while...

  // event->type=ALLOC;                                   // LtAdgs
  event->vaddr = __READ_ONCE(*vaddr);
  // printf("vaddr:%lu\n", event->vaddr);
  // tempmem+=(sizeof(unsigned long));
  event->flags = __READ_ONCE(*flags);
  // printf("flags:%lu\n", event->flags);
  // getchar();
  if (unlikely(event->flags & UFFD_PAGEFAULT_FLAG_SWAP))
    event->offst = __READ_ONCE(*paddr) * SYS_PAGE_SIZE;
  event->length = __READ_ONCE(*rem_size);
  // printf("offst:%lu\n", event->offst);
  // tempmem+=(sizeof(unsigned long));
  return 0;
}

int usm_mem_ret_ev_special(
    struct usm_event
        *event) { // add another indirection to treating channel in usm_event..
  int *status = (int *)(event->usmmem + sizeof(int));
  unsigned long *vaddr = (unsigned long *)(event->usmmem + 8); // sizeof(int)*2
  unsigned long *paddr =
      (unsigned long *)(event->usmmem +
                        16); // sizeof(int)*2+sizeof(unsigned long)
  unsigned long *flags =
      (unsigned long *)(event->usmmem +
                        24); // sizeof(int)*2+sizeof(unsigned long)*2

  unsigned long *rem_size = (unsigned long *)(event->usmmem + sizeof(int) * 3 +
                                              sizeof(unsigned long) * 3);

#ifdef DEBUG
  printf("[Spec.] Retrieving mem. based fault event\n");
#endif

  // event->origin=*(int*)event->usmmem;  // little specialy..
  // printf("Origin : %d\n", event->origin);
  // tempmem+=(sizeof(int)*2);       // can be done before putting in the event
  // or while...

  event->type = ALLOC;
  event->vaddr = *vaddr; // TODO : READ_ONCEify.. but test this all up for any
                         // perf. down....
  // printf("vaddr:%lu\n", event->vaddr);
  // tempmem+=(sizeof(unsigned long));
  event->flags = *flags;
  // printf("flags:%lu\n", event->flags);
  // getchar();
  if (unlikely(event->flags & UFFD_PAGEFAULT_FLAG_SWAP))
    event->offst = (*paddr) * SYS_PAGE_SIZE;
  event->length = *rem_size;
  // printf("offst:%lu\n", event->offst);
  // tempmem+=(sizeof(unsigned long));

  *status = 0; // only real specialty

  return 0;
}

int usm_mem_subm_ev(struct usm_event *event) {
  // usm_set_pfn_mem(event->origin,event->vaddr,event->paddr,0);     // TODO..
  // do ya..?
  // printf("Putting %lu PFN\n", event->paddr);
  int *status = (int *)(event->usmmem + sizeof(int));
  unsigned long *paddr =
      (unsigned long *)(event->usmmem +
                        16); // sizeof(int)*2+sizeof(unsigned long)
  unsigned long *rem_size =
      (unsigned long *)(event->usmmem + sizeof(int) * 3 +
                        sizeof(unsigned long) *
                            3); // hmm.. with this following thingy.. fuse..
                                // probability...
                                // unsigned long *flags = (unsigned long
                                // *)(event->usmmem+sizeof(int)*2+sizeof(unsigned
                                // long)*2);
  int *further_alloc =
      (int *)(event->usmmem + sizeof(int) * 3 + sizeof(unsigned long) * 4);
  unsigned long *extra_allocs_plh =
      (unsigned long *)(event->usmmem + sizeof(int) * 4 +
                        sizeof(unsigned long) * 4);
  // int *state = (int *)(event->usmmem+sizeof(int)*2+sizeof(unsigned long)*3);
  // // TODO'
  int res = 2;
  // tempmem+=(sizeof(int)*2)+sizeof(unsigned long);
  __WRITE_ONCE(*paddr, event->paddr); // *paddr=event->paddr;
#ifdef DEBUG
  printf("[mem] Submitting event..\n");
#endif
  // printf("Put %lu PFN\n", *paddr);
  // tempmem=event->usmmem;
  // tempmem+=sizeof(int);
  // if(event->length) {
  //*further_alloc=1;
  // list_for_each_entry().. nope, preprompting/placing thingies with another
  // helper.. and here, we just boom it out!
  //}
  __WRITE_ONCE(*further_alloc, event->length); // *further_alloc=event->length;
  __WRITE_ONCE(*status, 2);                    // *status=2;
  // printf("%lu", )
  res = __READ_ONCE(*status);
  while (res == 2) {
    usm_yield(); // TODO heavily important : io_uring'ify them fread/writes..
                 // and yield 'em up.       // Mayday.. only works if sched.c
                 // heavified/complexified a bit.. or popping done here...
                 // atomics toUSE? TODO inv.      .. hehehe, now always or
                 // gotten thingies from u_bitfield/com., sched.c wise!
    res = __READ_ONCE(
        *status); // TODO' process' crash case : if (__READ_ONCE(*state)) { res
                  // = 1;   break; }     // but dude.. it just can't crash
                  // here.. which friggin' MAGIC!? IT'S A DAMN SIMPLE KTASK...
                  // AIN'T NO GOTO ANYWHERE.. unless I'm freaking idiotic
                  // again..., hence (!) only yield.. and TODO eventually
                  // #ifdef'ify
  }
  // res = *status;
  event->flags =
      __READ_ONCE(*rem_size); // it is on the same worker.. so the guy just
                              // keeps on going, while usm freaking looks into
                              // everything.. but reads of unsigned long.. man..
  __WRITE_ONCE(*status, 0);
  // printf("Received %d from K.\n", res);
  if (res == 3)
    return 0;
  else
    return res; // ret. res then #define thingies to tell him what to do.. try
                // again or stuffs.. although that'd be with the other way o_o'?
}

int usm_mem_check(char *mem,
                  int timeout) { // ain't even relevant in this version...
  int status =
      __READ_ONCE(*(int *)(mem + sizeof(int))); // short dismissed for now..
  int state = *(int *)(mem + sizeof(int) * 2 +
                       sizeof(unsigned long) *
                           3); // sizeof(int)*2+sizeof(unsigned long)*3

  if (unlikely(state == 10)) { // find a way to put it afterwards..
    __WRITE_ONCE(*(int *)(mem + sizeof(int) * 2 + sizeof(unsigned long) * 3),
                 0); // duplicate of kernel code.. but here should be better..
                     // but ain't working.. -_-... cache and 'em..
    return -1;
  } // other way found.. this practically never gets hit...     // TODO comment
    // with state and see/inv.

  if (status == 1)
    return 1;
  else
    return 0;
  // bruh.. them kfrees on this mem.. we should keep the ref.s around ctx, and
  // free them all at fd's outing... as we won't be using this to detect
  // process' ciao ciao.
}

struct usm_channel_ops usm_cnl_userfaultfd_ops = {
    .usm_retrieve_evt = usm_uffd_ret_ev,
    .usm_submit_evt = usm_uffd_subm_ev,
    .usm_check = usm_poll_check};

struct usm_channel_ops usm_cnl_special_ops = {
    .usm_retrieve_evt = usm_mem_ret_ev_special,
    .usm_submit_evt = usm_uffd_subm_ev_iu,
    .usm_check_mem = usm_mem_check};

struct usm_channel_ops usm_cnl_emul_ops = {.usm_submit_evt =
                                               usm_uffd_subm_ev_emul};

struct usm_channel_ops usm_cnl_mem_ops = {.usm_retrieve_evt = usm_mem_ret_ev,
                                          .usm_submit_evt = usm_mem_subm_ev,
                                          .usm_check_mem = usm_mem_check};

struct usm_channel_ops usm_cnl_freed_ops = {
    .usm_retrieve_evt = usm_freed_page_ret_ev, .usm_check = usm_poll_check};

struct usm_channel_ops usm_cnl_new_process_ops = {
  // usm_init :
  usm_retrieve_evt : usm_new_proc_ret_ev,
  usm_check : usm_poll_check_np
  // usm_submit_evt:     usm_new_proc_subm_ev
};

// struct usm_channel usm_cnl_userfaultfd=
// {.usm_cnl_ops=usm_cnl_userfaultfd_ops};

struct usm_channel usm_cnl_freed_pgs;
struct usm_channel usm_cnl_nproc;

struct usm_worker usm_wk_uffd;
struct usm_worker usm_wk_free_pgs;
struct usm_worker usm_wk_nproc;
