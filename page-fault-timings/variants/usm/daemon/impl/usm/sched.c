#define _GNU_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "../../include/usm/usm.h" // #include "producer_consumer/app.h"

#define NCYCLESLEEP 100000

// #define printf(...) ;
/*#define MAX_UTHREADS 100
#define MAX_IUTHREADS 100
#define MAX_VCPUS 10*/
// #define STACK_SIZE 32768 // 8388608 // default thread stack size // 32768
// relocation truncated to fit: R_X86_64_PC32 against symbol

// int difficulty = 10000;
unsigned int nb_vcpus = 0;
// unsigned int num_uthreads = 0;
// unsigned char *address;
volatile uint8_t tripwire = 0;

/*typedef struct {
        ucontext_t task;
        int id;				//.. nec.?
        char task_stack [STACK_SIZE];
        struct usm_event event;
        int state;	// kinda duplicate.. uhh.
        // int fd;	// usm_event's origin
} vUthread_t;*/

vcpu_t vcpu_list[MAX_VCPUS]; // dyna. alloc. TODO

// int shm[7];
// volatile unsigned long long term[7];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*static inline unsigned int get_vcpu_id(void)
{
        unsigned int i;
        pthread_t thread = pthread_self();
        for (i = 0; i < nb_vcpus && vcpu_list[i].thread != thread; i++)
                ;
        return i;
}*/

// ucontext_t uctx_main;

#ifdef UINTR
void __attribute__((interrupt))
__attribute__((target("general-regs-only", "inline-all-stringops")))
scheduler_handler(struct __uintr_frame *ui_frame, unsigned long long vector) {
  (void)ui_frame;
  // (void)vector;

  if (likely(!vector)) {
    /*#ifdef DEBUG
            vcpu_t *cur_vcpu = &vcpu_list[get_vcpu_id()];
            printf("[Handling/Waked] %s awoken..!\n",
    cur_vcpu->wk_args->thread_name); //cur_vcpu->wk_args->thread_name); #endif*/	// deadlock inducing..
    return;
  }
#else
void scheduler_handler(int sig) {
  (void)sig; // a most likely meaningless version of time serving with
             // signals..? Maybe TODO		// .. and the function can
             // simply even be separated.. saving.. an.... if*.. ^^'
             // // ... aand, even already done.. meh.
#endif

  unsigned int vcpu_id = get_vcpu_id();
  vcpu_t *cur_vcpu = &vcpu_list[vcpu_id];
  unsigned int todo =
      cur_vcpu->tempEvents &
      (~((unsigned int)1 << cur_vcpu->current_uthread
                                ->id)); // is this even not just heavifying
                                        // thingies.. for a case prol'ly* quite
                                        // scarce..?		*not quite
  // here.. look up an id there (bitfield) and swap.. if naught, leave it at
  // that.
  /*int usmid = ffs(*(unsigned int *)cur_vcpu->wk_args->com)-1;

  if(usmid) {		// likelyOrNotON?
          int prev_uthread = cur_vcpu->current_uthread;
          int *cur_uthread = &cur_vcpu->current_uthread;
          *cur_uthread = usmid; // (*cur_uthread + 1) % num_uthreads;
#ifdef DEBUG
  printf("Soldier %s swapping %d to %d UGThreads!\n",
cur_vcpu->wk_args->thread_name, prev_uthread, *cur_uthread); #endif
          swapcontext(&cur_vcpu->uthreads[prev_uthread].task,
                          &cur_vcpu->uthreads[*cur_uthread].task);
  }
#ifdef DEBUG
  else {
          printf("Soldier %s attempted to swap to oblivion!\n",
cur_vcpu->wk_args->thread_name);
  }
#endif*/

  /* Man.. just give the hand to main.. he'll take care'f everything..! And just
   * use states..*/
  // but only if thingies needed to be done..? Yes, let's even ignore the fact
  // that it could be another hogger.. will still be quite faire anyway.
  if (todo)
    swapcontext(&cur_vcpu->current_uthread
                     ->task, // uthreads[cur_vcpu->current_uthread].task,
                &cur_vcpu->uctx_main);
}

#ifndef UINTR
/*void __attribute__((interrupt))
__attribute__((target("general-regs-only", "inline-all-stringops")))
waker_handler_local(struct __uintr_frame *ui_frame, unsigned long long vector)
{
        (void)ui_frame;
        (void)vector;
#else*/
void waker_handler_local(int sig) {
  (void)sig;
  // #endif
  /*#ifdef DEBUG
          vcpu_t *cur_vcpu = &vcpu_list[get_vcpu_id()];
          printf("[Handling/Waked] %s awoken..!\n",
  cur_vcpu->wk_args->thread_name); //cur_vcpu->wk_args->thread_name); #endif*/		// deadlock inducing..
}
#endif

/* Temp. placement.. */

void usm_mem_worker_handler_singular(
    int wk_args) { // use global uctx_main (but make it per vCPU later) and do
                   // swap_contexts here.. saves some precious time
  int chn =
      wk_args; // &usmMemWorkers; // (while i next until...) // (struct
               // usm_worker*) wk_args; ([IMP] uthreads' bug fix attempt...)
#ifdef DEBUG
  printf("Yo'p there! %d\n", chn);
#endif
  /*printf("Yo'p there! %d\n", chn);
      getchar();
      chn--;*/
  int vcpu_id = get_vcpu_id(); // temp..
  vcpu_t *cur_vcpu = &vcpu_list[vcpu_id];
  struct usm_worker *wk = cur_vcpu->wk_args;
  struct usm_channel *channel = &wk->usm_channels[chn];
#ifdef DEBUG
  printf("[First up]\tvcpu_id : %d\n", vcpu_id);
  getchar();
#endif
  // struct usm_event *event;    // lotta precharging TODO beforehand.. and not
  // here, as this treats of a lotta 'em.

  /*struct sigaction sa;
      sa.sa_flags = 0;
      sa.sa_sigaction = (void *)scheduler_handler;
      sigemptyset(&sa.sa_mask);
      sigaddset(&sa.sa_mask, SIGUSR2);
      sigaction(SIGUSR2, &sa, NULL);*/

  uthread_t *cur_uthread = &cur_vcpu->uthreads[chn];
  cur_uthread->state = 1;

  while (unlikely(!channel->event.usmmem)) {
#ifdef DEBUG
    printf("[%s] Halt! memChan-%d not yet registered!\tSHOULD NEVER HAPPEN!\n",
           wk->thread_name, chn); // ain't happening anymore..
    getchar();
#else
    ;
#endif
  }

  getcontext(&cur_uthread->task); // exactement
  while (1) {
    /*#ifdef DEBUG 			// pure lockless attempts : moving
    down.. printf("[uT%d/%s] Popping ID : %d, %u, %u\n", chn, wk->thread_name,
    chn, cur_vcpu->tempEvents, *wk->com); #endif
                    __WRITE_ONCE(*wk->com, *wk->com&(~((unsigned int)1 <<
    chn))); //*(unsigned long long *)wk->com&=~((unsigned long long)1 << id); //
    toggle it (ID) off (.. leave it) here or there..? #ifdef DEBUG
                    printf("[uT%d/%s] ID popped : %u, %u\n", chn,
    wk->thread_name, chn, *wk->com); #endif*/
    int ret = channel->usm_handle_evt(channel);
    if (likely(!ret)) {
/*#ifdef DEBUG 			// pure lockless attempts : moved down..
                        printf("[uT%d/%s] Popping ID : %d, %u, %u\n", chn,
wk->thread_name, chn, cur_vcpu->tempEvents, *wk->com); #endif
                        __WRITE_ONCE(*wk->com, *wk->com&(~((unsigned int)1 <<
chn))); //*(unsigned long long *)wk->com&=~((unsigned long long)1 << id);    //
toggle it (ID) off (.. leave it) here or there..? #ifdef DEBUG printf("[uT%d/%s]
ID popped : %u, %u\n", chn, wk->thread_name, chn, *wk->com); #endif*/
#ifdef DEBUG
      // {
      printf("Fault treated! %d\n", chn);
#ifndef NSTOP
      getchar();
#endif
      //}
#else
      ;
#endif
    } else {
      if (unlikely(ret == 2)) { // TODO #def dis 2
        cur_uthread->state = 0; // avoiding gotos.. these uthreads' intricacies
                                // be like booming anytime..
        swapcontext(&cur_uthread->task, &cur_vcpu->uctx_main);
        while (unlikely(!channel->event.usmmem)) {
#ifdef DEBUG
          printf("[%s] Halt/Reusage! memChan-%d not yet registered!\tSHOULD "
                 "NEVER HAPPEN!\n",
                 wk->thread_name, chn); // ain't happening anymore..
          getchar();
#else
          ;
#endif
        }
#ifdef IUTHREAD
        if (unlikely(cur_uthread->alt.manager.ioctl_fd !=
                     -1)) { // hmm.. going back through -2 ain't
                            // relevant/needed... ain't* it?
#ifdef DEBUG
          printf("Metamorphosin'..! To special!\n");
#endif
          /*return weirdo..tho'...*/ usm_mem_worker_handler_uts(
              chn /*wk_args*/);
        }
#endif
        continue;
      }
#ifdef DEBUG
      printf("Fault not treated! %d\n", chn);
      getchar();
#endif
      /*#else
                              ;		// sap'pari wakaranu'.. nani 'o shiro
      ka..? #endif*/
    }
    /*#ifdef DEBUG 			// pure lockless attempts : moving
    down..	|	..further moved down... : regardless of success or not,
    pop off.		| .. nope, locking for now.. temp. TODO
                    printf("[uT%d/%s] Popping ID : %d, %u, %u\n", chn,
    wk->thread_name, chn, cur_vcpu->tempEvents, *wk->com); #endif
                    __WRITE_ONCE(*wk->com, *wk->com&(~((unsigned int)1 <<
    chn))); //*(unsigned long long *)wk->com&=~((unsigned long long)1 << id); //
    toggle it (ID) off (.. leave it) here or there..? #ifdef DEBUG
                    printf("[uT%d/%s] ID popped : %u, %u\n", chn,
    wk->thread_name, chn, *wk->com); #endif*/
    cur_uthread->state = 0;
    // swapcontext(&cur_vcpu->uthreads[*cur_uthread].task, &uctx_main);
    // setcontext(&cur_vcpu->uctx_main);		// or just leave it.. it
    // has uc_link.... maybe TODO test diff. between the two..

    // look.. that swapping.. premempt ver. ... pushes things around.. and if it
    // was preempted in the middle of a fault, it won't get it to pop* thingies
    // when put back by main/man.r. ... so.. dis opti.. to ditch.. at least for
    // now.... TODO alm. done.
    swapcontext(&cur_uthread->task, &cur_vcpu->uctx_main);
  }
}

#ifdef IUTHREAD
void usm_mem_worker_handler_singular_special(int wk_args, int id) {
  int chn = wk_args;
  int vcpu_id = get_vcpu_id(); // temp..
  vcpu_t *cur_vcpu = &vcpu_list[vcpu_id];
  struct usm_worker *wk = cur_vcpu->wk_args;
  struct usm_channel *channel = &wk->usm_channels[chn];
  unsigned int *specCom = (unsigned int *)(((uintptr_t)channel->event.usmmem) +
                                           4096 - sizeof(unsigned int) * 2);
#ifdef DEBUG
  printf("[First up | UTSpecial / Managee%d]\tvcpu_id : %d\n", id, vcpu_id);
#endif
  // struct usm_event *event;    // lotta precharging TODO beforehand.. and not
  // here, as this treats of a lotta 'em.

  /*
          Dude, if the one to which the hand was given gets preempted, it simply
     means he's not meant to have it next.. hence, down here, just go next until
     back to it.. in a similar manner to the one described farther.. So, here,
     we just, for iuID, take the current Christine put iuID, as it won't budge
     until we read and put status back to 0. Hence, the reusal*'ll maybe be the
     problem, but that ID's to be conserved all throughout.

          But all that's not to be done here.. here's the super* main giving
     hands.. we'll makecontext when seeing an fd to 0, and swapcontext if not..
     all in an intern while loop...

          Moreover, current_uthread's to friggin' be a ucontext_t pointer man..
     dis induces another indirection which'll quite screw everything over if
     persisted in such direction..

          Gud. All dis to be flashdone tomorrow real quick, not even comparably
     to Quicksilver. Main thingie's that part'a mmap to take and read
     consequently.. and write... 4096-int*2.... we should quite already have it
     around wholly.
  */

  uthread_t *cur_man_uthread = &cur_vcpu->uthreads[chn];
  uthread_t *cur_uthread =
      (uthread_t *)(cur_man_uthread->alt.manager.iuthreads +
                    (id * sizeof(uthread_t)));

  cur_uthread->state = 1;

  while (unlikely(
      !cur_uthread->alt.event.origin)) { // while (unlikely(!channel->buff)) {
#ifdef DEBUG
    printf("[%s/Managee] Halt! specMemChan-%d/%d not yet registered!\tSHOULD "
           "NEVER HAPPEN!\n",
           wk->thread_name, id, chn); // ain't happening anymore..
    getchar();
#else
    ;
#endif
  }

  getcontext(&cur_uthread->task);
  while (1) {
    /*#ifdef DEBUG
                    printf("[suT%d/%d/%s] Popping (or not) spec. ID : %d, %u,
    %u\n", id, chn, wk->thread_name, id,
    cur_man_uthread->alt.manager.tempIUTEvents, *specCom);
                    // printf("[suT%d/%d/%s] Spec. ID popped : %u, %u\n", id,
    chn, wk->thread_name, chn, *specCom); #endif*/
    int ret = channel->usm_handle_evt(&cur_uthread->alt.event);
    if (!ret) {
      // __WRITE_ONCE(*specCom, *specCom&(~((unsigned int)1 << id)));
#ifdef DEBUG
      printf("[%s/%d/Managee'%d] Fault treated!", wk->thread_name, chn,
             id); //  Now** poppin'(ed*ed(done either way.. but down)) *** !
                  // getchar();
#endif
    } else {
      if (unlikely(ret == 2)) { // TODO #def dis 2
        cur_uthread->state = 0;
        swapcontext(&cur_uthread->task, &cur_vcpu->uctx_main);
        while (unlikely(
            !channel->event
                 .usmmem /*channel->buff*/)) { // usmmem mostly here.. but
                                               // samie.
                                               // // pure super duper dupra
                                               // duplicate (alt.event.usmmem
                                               // and event.usmmem.. event in
                                               // itself's needed to preserve
                                               // fault states between
                                               // Christine's instances.. but
                                               // usmmem.. either find it some
                                               // value or :>) TODO rid somehow.
#ifdef DEBUG
          printf("[%s] Halt/Reusage! memChan-%d not yet registered!\tSHOULD "
                 "NEVER HAPPEN!\n",
                 wk->thread_name, chn); // ain't happening anymore..
          getchar();
#else
          ;
#endif
        }

        // kinda bring this type's metamorphosis' code here.. TOOD

        continue;
      }
#ifdef DEBUG
      printf("[%s/%d/Managee'%d] Fault not treated!\n", wk->thread_name, id,
             chn);
      // getchar();
#endif
    }
    // __WRITE_ONCE(*specCom, *specCom&(~((unsigned int)1 << id)));
    // // mutexes' imagining.. them booming up on one, then we change it
    // inadvertently... dk if plausible enough, but might sum'r'll fix thingies
    // atm..		// now done lower ... TODO update lower
#ifdef DEBUG
    printf("\t%u\n", *specCom);
#ifndef NSTOP
    getchar();
#endif
#endif
    // #else
    //;		// sap'pari wakaranu'.. nani 'o shiro ka..?
    // #endif
    cur_uthread->state = 0;
    // swapcontext(&cur_vcpu->uthreads[*cur_uthread].task, &uctx_main);
    swapcontext(
        &cur_uthread->task,
        &cur_vcpu
             ->uctx_main); // setcontext(&cur_vcpu->uctx_main);		// or,
                           // again that guy..?		// Ye, mostly..!
  }
}

void usm_mem_worker_handler_uts(int wk_args /*, int ioctl_fd*/) {
  int chn = wk_args;
  int vcpu_id = get_vcpu_id(); // temp..
  vcpu_t *cur_vcpu = &vcpu_list[vcpu_id];
  struct usm_worker *wk = cur_vcpu->wk_args;
  struct usm_channel *channel = &wk->usm_channels[chn];
  uthread_t *cur_uthread;
  uthread_t *cur_iuthreads;

  cur_uthread = &cur_vcpu->uthreads[chn];

#ifdef DEBUG
  printf("[First up | UTSpecial / Manager]\tvcpu_id : %d, ioctl_fd : %d\n",
         vcpu_id, cur_uthread->alt.manager.ioctl_fd);
#endif

  /* Could be allocated here too.. */ // ye., should only be here possible **
                                      // (not in uthread_create_recursive.. as
                                      // that should be the fd Majordomo boi's)

  int ix = 0;

  cur_uthread->alt.manager.iuthreads =
      malloc(sizeof(uthread_t) * MAX_IUTHREADS);
  cur_iuthreads = (uthread_t *)cur_uthread->alt.manager.iuthreads;

  while (ix < MAX_IUTHREADS) {
    uthread_t *current_iuthread =
        &(cur_iuthreads[ix]); // &(cur_iuthreads+(ix*sizeof(uthread_t))); //
                              // &(cur_iuthreads[ix]);
    current_iuthread->id = ix++;
    // Initialize the task context
    CHECK_FUNC(getcontext(&current_iuthread->task) ==
               0); // unnecessary.. and nonsense. Not even here..&Co.
    current_iuthread->alt.event.origin =
        cur_uthread->alt.manager
            .ioctl_fd; // pseudo fd.. checked to see whether init.ed or ever
                       // used.. but :		| actually.. no need to do that
                       // init'ing "à la volée", as nuthin' should change
                       // anyway..! 'ioctl_fd
    current_iuthread->alt.event.usmmem = wk->usm_channels[chn].event.usmmem;
    current_iuthread->alt.event.type = ALLOC;
    current_iuthread->alt.event.channel = (void *)-1;
    current_iuthread->task.uc_stack.ss_sp = current_iuthread->task_stack;
    current_iuthread->task.uc_stack.ss_size = STACK_SIZE;
    current_iuthread->task.uc_link =
        &cur_vcpu->uctx_main; // or give back the hand to its local boss?
                              // i.e. &current_uthread->task

    makecontext(&current_iuthread->task,
                (void (*)())usm_mem_worker_handler_singular_special, 2, chn,
                current_iuthread->id);
  }

  uthread_t *iuthreads =
      (uthread_t *)cur_uthread->alt.manager
          .iuthreads; // TODO fix all these thingies' placements..

  cur_uthread->state = 1;

  while (unlikely(
      !channel->event
           .usmmem /*channel->buff*/)) { // now done through -2.. TODO rid.
#ifdef DEBUG
    printf("[%s] Halt! memChan-%d not yet registered!\tSHOULD NEVER HAPPEN!\n",
           wk->thread_name, chn); // ain't happening anymore..
    getchar();
#else
    ;
#endif
  }

  // cur_iuthreads = cur_vcpu->current_uthread;	// why even dis..

  getcontext(&cur_uthread->task);

  while (1) {
    // __WRITE_ONCE(*wk->com, *wk->com&(~((unsigned int)1 << chn)));
    // // the contention could be chaotic.. some spin_lock could be required..!
    // TODO inv.
    if (unlikely(*(unsigned int
                       *)((uintptr_t)(channel->event.usmmem /*channel->buff*/) +
                          SYS_PAGE_SIZE - sizeof(unsigned int) * 3) == 100)) {
#ifdef DEBUG
      printf("[SpecialAgent/Info.] Emulating a classical bad boi! Soldier 0 to "
             "special duty!\n");
#endif
      cur_uthread->state = 0;
      __WRITE_ONCE(*wk->com, *wk->com & (~((unsigned int)1 << chn)));
      /*swapcontext(&cur_uthread->task,
                      &iuthreads[0].task);*/
      /*setcontext(&iuthreads[0].task);
      setcontext(&cur_vcpu->uctx_main);	// ebver gotten btw..*/
      usm_handle_mem_special(&iuthreads->alt.event); // '[0] but same..
      setcontext(&cur_vcpu->uctx_main);
    }
  trtIUT:
    cur_uthread->alt.manager.tempIUTEvents =
        *(unsigned int *)cur_uthread->alt.manager
             .iUTcom; // here.. ya saw that dang cur_vcpu's wk.. TODO tighten up
    int ret;
    // and here
    /*if (!cur_vcpu->tempEvents) {      // __builtin_popcountll(tempEvents)
                    // yield... but that's a bug. Should never happen (with not
    cur_vcpu ofc..)
    }
    else {*/
    int treatOtherFaults = 0;
    // __WRITE_ONCE(*wk->com, *wk->com&(~((unsigned int)1 << chn)));
    // // should be here.. but testin'.. and actually not here but there before
    // trtIUT
    while (cur_uthread->alt.manager.tempIUTEvents) {
      unsigned int tempIUTEvents = cur_uthread->alt.manager.tempIUTEvents;
      while (tempIUTEvents) {
        int id = ffs(tempIUTEvents) -
                 1; // __lg(tempEvents);      // or just get that -1 before this
                    // all.. meh, should be the same.. -_-'.. let's print JIC..
        // rounds=0;
        while (unlikely(
            !iuthreads[id]
                 .alt.event
                 .origin /*!channelsList[id].buff*/)) { // legacy, but wanted at
                                                        // some point.. bin
                                                        // (trash) tho..
#ifdef DEBUG
          printf("[%s/Distributor/Inner%d] Halt! iUmemChan-%d not yet "
                 "registered!\t..ridiculous..\n",
                 wk->thread_name, cur_uthread->id, id);
          getchar();
#else
          ;
#endif
        }
#ifdef DEBUG
        printf("[%s/Inner%d] Treating iUTmem. event %d/%d \t (Could be swapped "
               "off (context!))\n",
               wk->thread_name, cur_uthread->id, id, chn);
#endif
        /* Some mutation thingy needed.. TOOD RN. ... pseudo placeholder per
         * this guy's instance? */
        cur_vcpu->current_uthread =
            &iuthreads[id]; // *cur_uthread = iuthreads[id]; 	// should do dis
                            // once switched to there.. hence sure we're saving
                            // good contexts.. but man	// TODO check dis
                            // weirdo..
        iuthreads[id].state = 1; //	cur_vcpu->uthreads[/**cur_uthread*/id].state
                                 //= 1;		// sadge. TODO
#ifdef DEBUG
        printf("Inner soldier %d of %s swapping mainiUT to %d/%d UGThreads!\n",
               cur_uthread->id, wk->thread_name, id, (&iuthreads[id])->id);
#endif
#ifdef TSERV
        rdtsc(wk->com + 3000 + sizeof(int) * 2);
#endif
        swapcontext(
            &cur_uthread->task,
            &iuthreads[id]
                 .task); // &(*cur_uthread.task)); //
                         // &cur_vcpu->uthreads[*cur_uthread/*+1*/].task);
        // *cur_uthread = 0;	uctx main, boom at 0, then everything else
        // starting from one.. just ++/-- all around..

        /*
                Here check for unfinished businesses.. and put them back in
           unless.. some priority concept (?).. or there again taking too much
           time : taken yet care again by handler. BY THE WAY! That stateful
           saving was friggin' right! Forgot but famine could happen if same
           most side bit gets triggered again and again.. so might just as well
           have to copy it, treat 'em all up, then continue..
        */

        // ret = channelsList[id].usm_handle_evt(&(channelsList[id])); //
        // usm_handle_mem(&(channelsList[id]));      // ret still not needed..
        // | tinyquanta pop it here.. if ret blablabla..     |   popped before
        // to make sure new faults get switched right in the bit field. |
        // Alain's suggestion.	// ..n.m.c'ed
        tempIUTEvents &= (~((unsigned int)1 << id));
        if (!iuthreads[id].state) {
          __WRITE_ONCE(cur_uthread->alt.manager.tempIUTEvents,
                       cur_uthread->alt.manager.tempIUTEvents &
                           (~((unsigned int)1 << id)));
        }
        /*if(!(cur_vcpu->tempEvents&(~((unsigned int)1 << id))))
           // some popcount and related play smwhr maybe receiveOtherFaults++;*/
      }
      if (treatOtherFaults++ /*==2*/)
        goto trtIUT;
    }
    if (likely(!*(unsigned int *)cur_uthread->alt.manager
                     .iUTcom)) { // basically basic but meh, should not be the
                                 // problem right about now..
      cur_uthread->state = 0;
      // __WRITE_ONCE(*wk->com, *wk->com&(~((unsigned int)1 << chn)));
      // // not so sure even with this if*		// AND EVEN FRIGGIN'
      // DONE BY MAIN man.. nope.. ain't done by him.. but us.		// now
      // down there .. bisTODO, update lower related.
    } else
      goto trtIUT;
#ifdef DEBUG
    printf("[SpecialAgent/Info.] Bunch'a bad bois treated!\n");
#ifndef NSTOP
    getchar();
#endif
#endif
    // swapcontext(&cur_vcpu->uthreads[*cur_uthread].task, &uctx_main);
    swapcontext(&cur_uthread->task,
                &cur_vcpu->uctx_main); // setcontext(&cur_vcpu->uctx_main);

    if (unlikely(cur_uthread->alt.manager.ioctl_fd ==
                 -1)) { // hmm.. going back through -2 ain't relevant/needed...
                        // is it?
#ifdef DEBUG
      printf("Metamorphosin'..! To classic singular!\n");
#endif
      /*return weirdo..tho'... */ usm_mem_worker_handler_singular(
          chn /*wk_args*/);
    }
  }
}

void usm_mem_worker_handler_alt(int wk_args) {
#ifdef DEBUG
  printf("[First up]\tAlt'Halt. %d\n", wk_args,
         (&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))->id);
  getchar();
#endif

  int tempfix = 0;
  while ((&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))
             ->alt.manager.ioctl_fd ==
         -2) // dis hence ensures.. that inner condition not to be needed.
             // // watchpoint..
#ifdef DEBUG
  {
    printf("%d\tNope! Init., mem_worker_handler! SNH..%d, %d/%d/%p. CPU ID : "
           "%d.\n",
           tempfix, wk_args,
           (&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))->id,
           (&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))
               ->alt.manager.ioctl_fd,
           &(&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))
                ->alt.manager.ioctl_fd,
           get_vcpu_id());
    getchar();
    if (tempfix++ == 5) {
      (&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))
          ->alt.manager.ioctl_fd = -1;
      printf("Assuming default.. temp. fix : %d",
             (&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))
                 ->alt.manager.ioctl_fd);
      getchar();
      break;
    }
  }
#else
    ;
#endif
  // (&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))->alt.manager.ioctl_fd =
  // -1;		// temporary..

#ifdef DEBUG
  printf("[First up]\tAlt. %d\n",
         (&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))
             ->alt.manager.ioctl_fd);
#ifndef NSTOP
  getchar();
#endif
#endif
  if ((&((&vcpu_list[get_vcpu_id()])->uthreads[wk_args]))
          ->alt.manager.ioctl_fd == -1)
    usm_mem_worker_handler_singular(wk_args);
  else
    usm_mem_worker_handler_uts(wk_args);
}
#endif

/*void *thread_func(void *arg)		// legacy..
{
        (void)arg;
        int vcpu_id = get_vcpu_id();
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(vcpu_list[vcpu_id].cpu_affinity, &mask);
        pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);

#ifdef UINTR
        CHECK_FUNC(!(uintr_register_handler(scheduler_handler, 0) < 0));
        vcpu_list[vcpu_id].upid_fd = uintr_create_fd(0, 0);
#endif
        CHECK_FUNC(!(vcpu_list[vcpu_id].upid_fd < 0));
#ifdef UINTR
        _stui();
#endif
        vcpu_list[vcpu_id].current_uthread = -1;
        vcpu_list[vcpu_id].state = 1;
#ifdef DEBUG
        printf("Soldier %d ready!\n", vcpu_id);
        getchar();
#endif
        while (1)
                ;
#ifdef DEBUG
        printf("Soldier %d weirdy!\n", vcpu_id);
#endif
        return NULL;
}*/

int uthread_create(int vCPU_id, int uthread_id, struct usm_worker *wk_args) {

  uthread_t *current_uthread = &vcpu_list[vCPU_id].uthreads[uthread_id];
  // int temp = 0;

  __WRITE_ONCE(current_uthread->alt.manager.ioctl_fd, -1);
  // current_uthread->alt.manager.ioctl_fd = -1;

  /*CHECK_FUNC(getcontext(&current_uthread->spin) == 0);
  current_uthread->spin.uc_stack.ss_sp = current_uthread->spin_stack;
  current_uthread->spin.uc_stack.ss_size = STACK_SIZE;
  current_uthread->spin.uc_link = NULL;*/

  /*// Initialize the task context
  CHECK_FUNC(getcontext(&current_uthread->task) == 0);
  current_uthread->task.uc_stack.ss_sp = current_uthread->task_stack;
  current_uthread->task.uc_stack.ss_size = STACK_SIZE;
  current_uthread->task.uc_link = &uctx_main; //&current_uthread->spin;*/

  /*if (uthread_id % 2 == 0)
  {*/
  // CHECK_FUNC(address = (uint8_t *)malloc(sizeof(uint8_t)));
  /*current_uthread->p_args.count = difficulty;
  current_uthread->p_args.address = address;*/

  // current_uthread->wk_args.vcpu_id = vCPU_id;		may be needed
  // later but not now..
  //*address = 0;

#ifdef DEBUG
  printf("Setting UThread %d's context!\n", uthread_id);
#endif

  /*while (temp<100) {		// done in specialized boi
          current_uthread->iuthreads[temp].event.origin = 0;	// dummy fd
  namie.		// .. make thingies NULL.. uhh? Some other placeholder
  to shoot down, as these ucontexts man.. o_o' ... o k a y , p r o b l e m o s
  starting o f f o . temp++;
  }*/

  /*makecontext(&current_uthread->task, (void (*)())usm_handle_evt_poll, 1,
                          &current_uthread->wk_args);*/
  /*makecontext(&current_uthread->task, (void
     (*)(void))usm_mem_worker_handler_singular, 1, uthread_id/*wk_args*//*);*/

#ifdef DEBUG
  printf("UThread %d's context set! %d, %d. #JK%p\n", uthread_id,
         current_uthread->alt.manager.ioctl_fd, current_uthread->id,
         &current_uthread->alt.manager.ioctl_fd);
#endif

  return 0;
}

#ifdef IUTHREAD
/* rid wk_args out */
int uthread_create_recursive(
    int vCPU_id, int uthread_id, struct usm_worker *wk_args,
    int ioctl_fd) // could'a just used the other one with some if(s)..
{

  uthread_t *current_uthread = &vcpu_list[vCPU_id].uthreads[uthread_id];
  struct usm_worker *wk_args_i = vcpu_list[vCPU_id].wk_args;

#ifdef DEBUG
  printf("Setting recursive UThread %d's context!\tiofd :%d, vCPU_id : %d\n",
         uthread_id, ioctl_fd, vCPU_id);
#endif
  current_uthread->alt.manager.iUTcom =
      (unsigned int *)(((uintptr_t)wk_args_i->usm_channels[uthread_id]
                            .event.usmmem) +
                       4096 - sizeof(unsigned int) * 2);

  current_uthread->alt.manager.ioctl_fd =
      ioctl_fd; // new.. more sensed.. hence TODO rid dis out..
  /*makecontext(&current_uthread->task, (void (*)())usm_mem_worker_handler_uts,
     2, uthread_id, ioctl_fd);*/

#ifdef DEBUG
  printf("Recursive UThread %d's context set! #JK\n", uthread_id);
#endif

  return 0;
}
#endif

void *soldiers(void *wk_args) {
  struct usm_worker *wk = (struct usm_worker *)wk_args;
  vcpu_t *cur_vcpu = &vcpu_list[wk->id]; // TODO there ^

  cur_vcpu->thread = wk->thread_id;
  cur_vcpu->wk_args = wk;
  struct usm_channel *channelsList = wk->usm_channels;

#ifdef UINTR
  int fd_vec;
  if (uintr_register_handler(scheduler_handler, 0) < 0) {
    perror("reg_hand_uipi ");
    abort();
  }
  fd_vec = uintr_create_fd(0, 0);
  if (fd_vec < 0) {
    perror("fd_vec ");
    abort();
  }
  __WRITE_ONCE(*(int *)((uintptr_t)(wk->com) + 2048 + sizeof(int)), fd_vec);
  fd_vec = uintr_create_fd(1, 0);
  if (fd_vec < 0) {
    perror("fd_vec_one ");
    abort();
  }
  __WRITE_ONCE(
      *(int *)((uintptr_t)(wk->com) + 2048 + sizeof(int) + sizeof(int)),
      fd_vec);
  _stui();
#else
  struct sigaction sa, sad; //, sag;
  sa.sa_flags = 0;
  sa.sa_sigaction = (void *)scheduler_handler;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGUSR2);
  // sigaddset(&sa.sa_mask, SIGALRM);
  sigaction(SIGUSR2, &sa, NULL);
  sad.sa_flags = 0;
  sad.sa_sigaction = (void *)waker_handler_local;
  sigemptyset(&sad.sa_mask);
  sigaddset(&sad.sa_mask, SIGUSR1);
  // sigaddset(&sa.sa_mask, SIGALRM);
  sigaction(SIGUSR1, &sad, NULL);
  /*sag.sa_flags = 0;
  sag.sa_sigaction = (void *)scheduler_handler;
  sigemptyset(&sag.sa_mask);
  sigaddset(&sag.sa_mask, SIGCHLD);
  sigaction(SIGCHLD, &sag, NULL);*/
#endif
  __WRITE_ONCE(*(int *)((uintptr_t)(wk->com) + 2048), getpid());
#ifdef DEBUG
  printf("[Sys] Wrote pid %d, %d!\n", getpid(),
         *(int *)((uintptr_t)(wk->com) + 2048));
#endif

  int rounds = 0;

  /*while(!*(unsigned int*)wk->com)	// ..not locally anymore.. *(unsigned
     int*)wk->com=0;  		// unsigned long long for max but....
              ;*/

  while (rounds < MAX_UTHREADS) {
    // wk->usm_channels[rounds].usm_cnl_id = wk->id;
    uthread_t *current_uthread = &cur_vcpu->uthreads[rounds];
    current_uthread->id = rounds++;
    current_uthread->alt.manager.ioctl_fd = -2;
    // Initialize the task context
    CHECK_FUNC(getcontext(&current_uthread->task) ==
               0); // unnecessary.. and nonsense.
    current_uthread->task.uc_stack.ss_sp =
        current_uthread->task_stack /* + STACK_SIZE*/;
    current_uthread->task.uc_stack.ss_size = STACK_SIZE;
    current_uthread->task.uc_link =
        &cur_vcpu->uctx_main; //&current_uthread->spin;
#ifdef IUTHREAD
    makecontext(&current_uthread->task,
                (void (*)(void))usm_mem_worker_handler_alt /*_singular*/, 1,
                rounds - 1 /*uthread_id*/ /*wk_args*/);
#else
    makecontext(&current_uthread->task,
                (void (*)(void))usm_mem_worker_handler_singular, 1,
                rounds - 1 /*uthread_id*/ /*wk_args*/);
#endif
  }

  // uthread_t *cur_uthread = cur_vcpu->current_uthread;

  rounds = 0;

/*#ifdef UINTR
        CHECK_FUNC(!(uintr_register_handler(scheduler_handler, 0) < 0));
        vcpu_list[wk->id].upid_fd = uintr_create_fd(0, 0);
#endif
        CHECK_FUNC(!(vcpu_list[wk->id].upid_fd < 0));*/
#ifdef UINTR
  _stui();
#endif

  getcontext(&cur_vcpu->uctx_main); // ineff..?
nground: // temp., inside loop.. just for momentary style* purposes..
#ifdef DEBUG
  printf("[%s] New round!\n", wk->thread_name);
#endif
  while (1) {
    // here
  rcv:
    cur_vcpu->tempEvents =
        *(unsigned int *)wk->com; // unsigned long long for max but...     //
                                  // unnecessary*(conserving for now for the
                                  // next copy needed..).. TODO | junction..
    int ret;
    // and here
    if (!cur_vcpu->tempEvents) { // __builtin_popcountll(tempEvents)
#ifdef DEBUG
      // printf("[%s] Naught to do..\n", wk->thread_name);
      ;
#endif
#ifndef NOSLEEP
      if (rounds == NCYCLESLEEP) {
#ifdef DEBUG
        printf("[%s] zZz..\n", wk->thread_name);
#endif
        rounds = 0;
        __WRITE_ONCE(*(wk->state), 0);
#ifdef UINTR
#ifdef DEBUG
        int res =
            uintr_wait(); // syscall(__NR_uintr_wait, 0 /*1000000000*/, 0);
        if (res < 0)
          perror("uintr_wait : ");
#else
        uintr_wait(); // syscall(__NR_uintr_wait, 0 /*1000000000*/, 0);
#endif
#else
        pause(); // return NULL; // swap_context.. for uthread's ver.
#endif
        // wk->state=1;        // duplicata.. o_o'
        goto nground;
      }
      rounds++;
#endif
    } else {
      int receiveOtherFaults = 0;
      while (cur_vcpu->tempEvents) {
        unsigned int tempEvents = cur_vcpu->tempEvents;
        while (tempEvents) {
          int id =
              ffs(tempEvents) -
              1; // __lg(tempEvents);      // or just get that -1 before this
                 // all.. meh, should be the same.. -_-'.. let's print JIC..
                 /*if(unlikely(id >= MAX_UTHREADS)) {		// should never happen
       after MAX_UTHREADS' compliance.. and there's even kernel side reassuring code..
                         printf("[%s/Distributor] ID %d exceeding troups' capacity!
       Droppin'!\n", wk->thread_name, id);        #ifdef DEBUG        getchar();        #endif
                         __WRITE_ONCE(*wk->com, *wk->com&(~((unsigned int)1 << id)));
                         __WRITE_ONCE(cur_vcpu->tempEvents,
       cur_vcpu->tempEvents&(~((unsigned int)1 << id)));        continue;
                 }*/

          /*while (unlikely(!channelsList[id].event.usmmem)) {	// buff
usm_handle_evt		// duplicate of inner behavior, and dis should even
disappear at some point #ifdef DEBUG printf("[%s/Distributor] Halt! memChan-%d
not yet registered!\tSHOULD NEVER HAPPEN!\n", wk->thread_name, id); getchar();
#else
                  ;
#endif
          }*/
#ifdef DEBUG
          printf("[%s] Treating mem. event %d \t (Could be swapped off "
                 "(context!))\n",
                 wk->thread_name, id);
#endif
          cur_vcpu->current_uthread =
              &cur_vcpu->uthreads[id]; //(*cur_uthread + 1) % num_uthreads;
                                       //// TODO check dis weirdo..
          cur_vcpu->current_uthread->state = 1; //	cur_vcpu->uthreads[/**cur_uthread*/id].state
                                                //= 1;		// sadge. TODO
#ifdef DEBUG
          printf("Soldier %s swapping main to %d UGThreads!\n", wk->thread_name,
                 id /**cur_uthread*/);
#endif
#ifdef TSERV
          rdtsc(wk->com + 3000 + sizeof(int) * 2);
#endif
          swapcontext(
              &cur_vcpu->uctx_main,
              &cur_vcpu->current_uthread
                   ->task); // &(*cur_uthread.task)); //
                            // &cur_vcpu->uthreads[*cur_uthread/*+1*/].task);
          // *cur_uthread = 0;	uctx main, boom at 0, then everything else
          // starting from one.. just ++/-- all around..

          /*
                  Here check for unfinished businesses.. and put them back in
             unless.. some priority concept (?).. or there again taking too much
             time : taken yet care again by handler. BY THE WAY! That stateful
             saving was friggin' right! Forgot but famine could happen if same
             most side bit gets triggered again and again.. so might just as
             well have to copy it, treat 'em all up, then continue..
          */

          // ret = channelsList[id].usm_handle_evt(&(channelsList[id])); //
          // usm_handle_mem(&(channelsList[id]));      // ret still not needed..
          // | tinyquanta pop it here.. if ret blablabla..     |   popped before
          // to make sure new faults get switched right in the bit field. |
          // Alain's suggestion.
          tempEvents &= (~((unsigned int)1 << id));
          if (!cur_vcpu->uthreads[id].state) {
            cur_vcpu->tempEvents &=
                (~((unsigned int)1
                   << id)); // am I thaat of a fool..? 'Nope, actually a genius,
                            // masha'Allah.		// Hmm.. a bit of a fool
                            // too, masha'Allah.. no __WRITE_ONCE needed..!
          }
          /*if(!(cur_vcpu->tempEvents&(~((unsigned int)1 << id))))
             // some popcount and related play smwhr maybe
                  receiveOtherFaults++;*/
        }
        if (receiveOtherFaults++ /*==2*/)
          goto rcv;
      }
    }
  }
}

int setSched() {
  // uint64_t time = atoll(argv[1]);
  // num_uthreads = 2;				// ain't need it.. lemme show
  // ya.		|.. welp..
  int baseAffinity = 0;

  nb_vcpus =
      1; //	= argc - 2;		later.. configurable... nbWorkers?
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(baseAffinity++, &mask);

  /*struct usm_worker *workersIterator = &usmWorkers;
int wk_nr = 0;
while(workersIterator) {
          (&vcpu_list[0].uthreads[wk_nr])->wk_args = (*workersIterator);
  workersIterator=workersIterator->next;
#ifdef DEBUG
  printf("UThread's args %d set!\n", wk_nr);
#endif
  wk_nr++;
  if(wk_nr==workersNumber+1)
      break;
}

  num_uthreads=wk_nr;		// welp.. temp..
  */

  /*pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);

  for (unsigned int i = 0; i < nb_vcpus; i++)
  {
          vcpu_list[i].state = 0;
          vcpu_list[i].cpu_affinity = baseAffinity++;		// make this
  freaking capped by real CPUs there.. retrieve it up.
          CHECK_FUNC(!(pthread_create(&vcpu_list[i].thread, NULL, thread_func,
  &i) < 0));
  }

  int pret = 0;		// legacy #tK
  while (!pret)
  {
          pret = 1;
          for (unsigned int i = 0; i < nb_vcpus; i++)
          {
                  if (vcpu_list[i].state == 0)
                  {
                          pret = 0;
                  }
          }
  }*/

#ifdef UINTR
  int fd_vec;
  if (uintr_register_handler(scheduler_handler, 0) < 0) {
    perror("reg_hand_uipi ");
    abort();
  }
  fd_vec = uintr_create_fd(0, 0);
  if (fd_vec < 0) {
    perror("fd_vec ");
    abort();
  }
  __WRITE_ONCE(*(int *)((uintptr_t)(usmMemWorkers.com) + 2048 + sizeof(int)),
               fd_vec);
#ifdef TSERV
  fd_vec = uintr_create_fd(1, 0);
  if (fd_vec < 0) {
    perror("fd_vec_one ");
    abort();
  }
  __WRITE_ONCE(*(int *)((uintptr_t)(usmMemWorkers.com) + 2048 + sizeof(int) +
                        sizeof(int)),
               fd_vec);
#endif
  _stui();
#endif
  __WRITE_ONCE(*(int *)((uintptr_t)(usmMemWorkers.com) + 2048), getpid());
#ifdef DEBUG
  printf("[Sys] Wrote pid %d, %d!\n", getpid(),
         *(int *)((uintptr_t)(usmMemWorkers.com) + 2048));
#endif

#ifndef UINTR
  struct sigaction sa, sad; //, sag;
  sa.sa_flags = 0;
  sa.sa_sigaction = (void *)scheduler_handler;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGUSR2);
  // sigaddset(&sa.sa_mask, SIGALRM);
  sigaction(SIGUSR2, &sa, NULL);
  sad.sa_flags = 0;
  sad.sa_sigaction = (void *)waker_handler_local;
  sigemptyset(&sad.sa_mask);
  sigaddset(&sad.sa_mask, SIGUSR1);
  // sigaddset(&sa.sa_mask, SIGALRM);
  sigaction(SIGUSR1, &sad, NULL);
  /*sag.sa_flags = 0;
  sag.sa_sigaction = (void *)scheduler_handler;
  sigemptyset(&sag.sa_mask);
  sigaddset(&sag.sa_mask, SIGCHLD);
  sigaction(SIGCHLD, &sag, NULL);*/
#endif

  // get it out.. leave it there...
  struct usm_worker *workersIterator = &usmWorkers;
  int wk_nr = 0;
  while (workersIterator) {
    // (&vcpu_list[0].uthreads[wk_nr])->wk_args = (*workersIterator);
    pthread_create(&(workersIterator->thread_id), NULL, usm_handle_evt_poll,
                   (void *)workersIterator); // uthread_create(0 /* temp. */,
                                             // wk_nr, workersIterator);
#ifdef DEBUG
    printf("Worker %d launched!\n",
           wk_nr); // printf("UThread's args %d 'kinda' set!\n", wk_nr);
#endif
    workersIterator = workersIterator->next;
    wk_nr++;
    if (wk_nr == workersNumber + 1)
      break;
  }
  // pthread_create(&(usmMemWorkersWatch.thread_id), NULL,
  // usm_mem_workers_waker, (void*)&usmMemWatchList);	// some waking
  // indrection/polarization* stuff maybe but meh no.. though we'll need another
  // thread which signals everyone periodically.. but shouldn't signal if state
  // changed.. so just make everyone touch it, and if untouched after rdtsc*,
  // signal... num_uthreads=1; // .. stub'by.. // num_uthreads=wk_nr;

  // uint64_t sstart, start, stop, current;
  /*for (unsigned int i = 0; i < nb_vcpus; i++)
  {
          term[i] = 0;
  }*/

  // rdtsc(&sstart);

  // rdtsc(&start);

#ifdef SYNC
  // flush all
  /*for (unsigned int i = 0; i < nb_vcpus; i++)
  {
          shm[i] = 0;
  }*/
#endif

  // notify all
  /*while (1) {		// to use for multiple workers../vCPUs.
          printf("[sysInfo] New round!\n");
          for (unsigned int i = 0; i < nb_vcpus; i++)
          {
#ifdef UINTR
                  // _senduipi(i);	// .. samie...
#else
                  // pthread_kill(vcpu_list[i].thread, SIGUSR1);
// this.. will... be sent... by the time server.... ''.. but dude, best of luck
to get 'em accurately, them friggin' timers.. #endif #ifdef DEBUG
                  //printf("Triggered vCPU %d!\n", i);
#endif
          }
          sleep(3);
  }*/

  /*
          This to be in a thread func., and launched if predefined number bigger
     than 1... AND if load high enough.. but yeah, we're playing it right here:
     each thread's his own watcher.. as also gets the externalization/union of
     wakers done right
  */

  vcpu_t *cur_vcpu = &vcpu_list[0]; // TODO there ^

  struct usm_worker *wk =
      &usmMemWorkers; // theeen the main became the first worker.. lul.
  cur_vcpu->thread = (pthread_t)
      pthread_self(); // ..			 | TODO.. man, double call out.
  wk->thread_id = (pthread_t)
      pthread_self(); // officially here! Baptism completed.
                      // pthread_create(&(usmMemWorkers.thread_id), NULL ,
                      // usm_mem_worker_handler, (void*)&usmMemWorkers);
                      // //&(workersIterator->attrs)
  cur_vcpu->wk_args = wk;
  struct usm_channel *channelsList = wk->usm_channels;

  int rounds = 0;

  while (rounds < MAX_UTHREADS) {
    uthread_t *current_uthread = &cur_vcpu->uthreads[rounds];
    current_uthread->id = rounds++;
    current_uthread->alt.manager.ioctl_fd = -2;
    channelsList[current_uthread->id].event.type =
        ALLOC; // others.. related to usm_handle_fd... such as FREE&Co.
    // Initialize the task context
    CHECK_FUNC(getcontext(&current_uthread->task) ==
               0); // unnecessary.. and nonsense.
    current_uthread->task.uc_stack.ss_sp =
        current_uthread->task_stack + STACK_SIZE;
    current_uthread->task.uc_stack.ss_size = STACK_SIZE;
    current_uthread->task.uc_link =
        &cur_vcpu->uctx_main; //&current_uthread->spin;
#ifdef IUTHREAD
    makecontext(&current_uthread->task,
                (void (*)(void))usm_mem_worker_handler_alt /*_singular*/, 1,
                rounds - 1 /*uthread_id*/ /*wk_args*/);
#else
    makecontext(&current_uthread->task,
                (void (*)(void))usm_mem_worker_handler_singular, 1,
                rounds - 1 /*uthread_id*/ /*wk_args*/);
#endif
    // CHECK_FUNC(getcontext(&current_uthread->task) == 0);
  }

#ifdef DEBUG
  printf("%d uthreads pre-init'ed!\n", MAX_UTHREADS);
#endif

  // uthread_t *cur_uthread = cur_vcpu->current_uthread;

  /*while(!*(unsigned int*)wk->com)	// ..not locally anymore.. *(unsigned
     int*)wk->com=0;  		// unsigned long long for max but....
          ;*/

  rounds = 0;

  getcontext(&cur_vcpu->uctx_main); // ineff..?
// nground:    // temp., inside loop.. just for momentary style* purposes..
#ifdef DEBUG
  printf("[%s] New round!\n", wk->thread_name);
#endif
  while (1) {
    // here
  rcv:
    cur_vcpu->tempEvents =
        cur_vcpu->tempEvents |
        __READ_ONCE(*(unsigned int *)
                         wk->com); // unsigned long long for max but...     //
                                   // unnecessary*(conserving for now for the
                                   // next copy needed..).. TODO | junction..
    int ret;
    // and here
    if (!cur_vcpu->tempEvents) { // __builtin_popcountll(tempEvents)
#ifdef DEBUG
      // printf("[%s] Naught to do..\n", wk->thread_name);
      ;
#endif
#ifndef NOSLEEP
      if (rounds == NCYCLESLEEP) {
#ifdef DEBUG
        printf("[%s] zZz..\n", wk->thread_name);
#endif
        rounds = 0;
        __WRITE_ONCE(*(wk->state), 0);
#ifdef DEBUG
        printf("[%s] zZz..good night!\t%d\n", wk->thread_name, *(wk->state));
#endif
#ifdef UINTR
#ifdef DEBUG
        if (uintr_wait() < 0)
          perror("uintr_wait : ");
#else
        uintr_wait(); // syscall(__NR_uintr_wait, 0 /*1000000000*/, 0);
#endif
#else
        // sigsuspend();	 TODO
        pause(); // return NULL; // swap_context.. for uthread's ver.
#endif
        __WRITE_ONCE(*(wk->state),
                     1); // wk->state=1;        // duplicata.. o_o'
                         // // ye but temporary trial..		| really
                         // short-lived leg.
        continue;        // goto nground;
      }
      rounds++;
#endif
    } else {
      int receiveOtherFaults = 0;
      while (cur_vcpu->tempEvents) {
        unsigned int tempEvents = cur_vcpu->tempEvents;
        while (tempEvents) {
          int id =
              ffs(tempEvents) -
              1; // __lg(tempEvents);      // or just get that -1 before this
                 // all.. meh, should be the same.. -_-'.. let's print JIC..
                 /*if(unlikely(id >= MAX_UTHREADS)) {		// should never happen
       after MAX_UTHREADS' compliance.. and there's even kernel side reassuring code..
                         printf("[%s/Distributor] ID %d exceeding troups' capacity!
       Droppin'!\n", wk->thread_name, id);        #ifdef DEBUG        getchar();        #endif
                         __WRITE_ONCE(*wk->com, *wk->com&(~((unsigned int)1 << id)));
                         __WRITE_ONCE(cur_vcpu->tempEvents,
       cur_vcpu->tempEvents&(~((unsigned int)1 << id)));        continue;
                 }*/

          /*while (unlikely(!channelsList[id].event.usmmem)) {	// buff
usm_handle_evt		// duplicate of inner behavior, and dis should even
disappear at some point #ifdef DEBUG printf("[%s/Distributor] Halt! memChan-%d
not yet registered!\tSHOULD NEVER HAPPEN!\n", wk->thread_name, id); getchar();
#else
                  ;
#endif
          }*/
#ifdef DEBUG
          printf("[%s] Treating mem. event %d \t (Could be swapped off "
                 "(context!))\n",
                 wk->thread_name, id);
#endif
          cur_vcpu->current_uthread =
              &cur_vcpu->uthreads[id]; //(*cur_uthread + 1) % num_uthreads;
                                       //// TODO check dis weirdo..
          cur_vcpu->current_uthread->state = 1; //	cur_vcpu->uthreads[/**cur_uthread*/id].state
                                                //= 1;		// sadge. TODO
#ifdef DEBUG
          printf("Soldier %s swapping main to %d UGThreads!\n", wk->thread_name,
                 id /**cur_uthread*/);
#endif
#ifdef TSERV
          rdtsc(wk->com + 3000 + sizeof(int) * 2);
#endif
          swapcontext(
              &cur_vcpu->uctx_main,
              &cur_vcpu->current_uthread
                   ->task); // &(*cur_uthread.task)); //
                            // &cur_vcpu->uthreads[*cur_uthread/*+1*/].task);
          // *cur_uthread = 0;	uctx main, boom at 0, then everything else
          // starting from one.. just ++/-- all around..

          /*
                  Here check for unfinished businesses.. and put them back in
             unless.. some priority concept (?).. or there again taking too much
             time : taken yet care again by handler. BY THE WAY! That stateful
             saving was friggin' right! Forgot but famine could happen if same
             most side bit gets triggered again and again.. so might just as
             well have to copy it, treat 'em all up, then continue..
          */

          // ret = channelsList[id].usm_handle_evt(&(channelsList[id])); //
          // usm_handle_mem(&(channelsList[id]));      // ret still not needed..
          // | tinyquanta pop it here.. if ret blablabla..     |   popped before
          // to make sure new faults get switched right in the bit field. |
          // Alain's suggestion.
          tempEvents &= (~((unsigned int)1 << id));
          if (!cur_vcpu->uthreads[id].state) {
            cur_vcpu->tempEvents &=
                (~((unsigned int)1
                   << id)); // am I thaat of a fool..? 'Nope, actually a genius,
                            // masha'Allah.		// Hmm.. a bit of a fool
                            // too, masha'Allah.. no __WRITE_ONCE needed..!
          }
          /*if(!(cur_vcpu->tempEvents&(~((unsigned int)1 << id))))
             // some popcount and related play smwhr maybe
                  receiveOtherFaults++;*/
        }
        if (receiveOtherFaults++ /*==2*/)
          goto rcv;
      }
    }
  }

  // wait
  /*rdtsc(&current);
  while (current - start < time)
  {
          rdtsc(&current);
  }*/
  // rdtsc(&stop);

  // fprintf(stdout, "%ld\n", (stop - sstart));
  return 0;
}
