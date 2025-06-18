#include <linux/userfaultfd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>

#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <pthread.h>
#include <signal.h>

#include <assert.h>

#include <poll.h>
#include <sched.h>

#include <stdbool.h>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define __WRITE_ONCE(x, val)                                                   \
  do {                                                                         \
    *(volatile typeof(x) *)&(x) = (val);                                       \
  } while (0)

#define __READ_ONCE(x) (*(const volatile typeof(x) *)&(x))

#define SYS_PAGE_SIZE sysconf(_SC_PAGESIZE)

#ifdef UINTR
#include <x86gprintrin.h>

// System call numbers for UINTR related functions
#define __NR_uintr_register_handler 471
#define __NR_uintr_vector_fd 473
#define __NR_uintr_register_sender 474

// Macros to call system calls for UINTR functionality                  // only
// one diff., so maybe reuse utils.h here.., with TSERV (no.. mfence here... but
// not super duper sure..)
#define uintr_register_handler(handler, flags)                                 \
  syscall(__NR_uintr_register_handler, handler, flags)
#define uintr_create_fd(vector, flags)                                         \
  syscall(__NR_uintr_vector_fd, vector, flags)
#define uintr_register_sender(fd, flags)                                       \
  syscall(__NR_uintr_register_sender, fd, flags)

// int glob = 0;
#endif

#ifdef TSERV
uint64_t tme;
static inline int dpreempt(uint64_t *dtime) {
  uint64_t ltime;
  __asm__ __volatile__("lfence\n");
  ltime = _rdtsc();
  __asm__ __volatile__("lfence\n");
  if (ltime - *dtime >= tme) // unlikely?
    return 1;
  return 0;
}
#endif

struct usm_watch_list {
  unsigned int
      *com; // char*! Temp. ... and should we just give out usm_worker..?
  int pid;  // pthread_t *thread_id;     // some way to directly pinpoint 'em
           // from here ('course with some indirection as is)'d be dope...
           // idk.... è-é.
#ifdef TSERV
  int timer;
#endif
  bool *state;
  // int *alive;
  // pthread_mutex_t lock;    let's try out lockless..
  struct usm_watch_list *next;
} usmMemWatchList;

void *
mapper(void *wk_args) { // call it logicMaintainer (or list.. meh, too classic)
                        // and call it whenever poll's up to smthn... TODO.. no
                        // real need to shoot ourselves up with threadies.. this
                        // is supposed to be straightforward, anyway.. so them
                        // mutexes? Nope. But.. ye but.. I'll just get said
                        // again "IT'S ONE IF!" -_-' and so on.. while if we use
                        // two CPUs.. kinda cheat.. uhhh mehsh.
  int usmSleepersFd = *(int *)wk_args;
  struct pollfd pollfd[1];
  pollfd[0].fd = usmSleepersFd;
  pollfd[0].events = POLLIN | POLLPRI;
#ifdef DEBUG
  printf("usmSleepersFd : %d\n", usmSleepersFd);
#endif
  while (1) {
    struct usm_watch_list *sailor =
        &usmMemWatchList; // could statefully preserve last boi but meh, think
                          // that out booming first before any even dope thing.

    /* int ret = poll(pollfd, 1, -1);       legacy..
    if(ret < 0) {
#ifdef DEBUG
        printf("No one's up anymore..! Ciaossu!");
#endif
        exit(EXIT_SUCCESS);
    }
    if (pollfd[0].revents == POLLIN) {
        printf("Yo! Reggin' some distant sleeper!\n");
        while(sailor->next)
            sailor=sailor->next;
        sailor->next=(struct usm_watch_list *)malloc(sizeof(struct
usm_watch_list)); sailor=sailor->next; sailor->com=(unsigned int *)mmap(NULL,
SYS_PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, usmSleepersFd, 0);      // **
MAP_FIXED if(sailor->com == MAP_FAILED) {    // NULL it up before..? Malloc
intricacies.. #ifdef DEBUG printf("[Mayday] Mapping failed! Aborting!"); #endif
            // close..?
            abort();
        }
        sailor->pid=*(int *)((uintptr_t)sailor->com+2048);
#ifdef UINTR
        sailor->pid=syscall(SYS_pidfd_getfd,sailor->pid, *(int
*)((uintptr_t)sailor->com+2048+sizeof(int)), 0);
        uintr_register_sender(sailor->pid, 0);
        // sailor->pid=glob++;
#endif
        sailor->state=(bool *)((uintptr_t)sailor->com+4092);
    }
    else {
        if(likely(pollfd[0].revents == POLLPRI)) {
#ifdef DEBUG
            printf("Yo! Unreggin' some distant sleeper!\n");
#endif
            struct usm_watch_list *tempList = &usmMemWatchList;
            while (tempList) {
                if(*(int *)((uintptr_t)(tempList->com)+3000) != 2)
                    goto next;
                void *toUnmap = tempList->com;
                tempList->com = NULL;
                munmap(toUnmap, SYS_PAGE_SIZE);
next:
                tempList=tempList->next;
            }
        }
        else {
#ifdef DEBUG
            printf("Unrecognized poll val., aborting!\n");
#endif
            abort();
        }
    }*/

    int ret;
    if (unlikely(
            (ret = poll(
                 pollfd, 1,
                 -1 /*!*/)))) { // let a mapper thread do this? But the
                                // locks...? But the timer's timings' falsing
                                // possibilites as is....?  TODO, base, IMP.
      if (ret < 0) {
#ifdef DEBUG
        printf("No one's up anymore..! Ciaossu!");
#endif
        exit(EXIT_SUCCESS);
      }
      struct usm_watch_list *sailor =
          &usmMemWatchList; // could statefully preserve last boi but meh, think
                            // that out booming first before any even dope
                            // thing.
      if (pollfd[0].revents == POLLIN) {
#ifdef DEBUG
        printf("Yo! Reggin' some distant sleeper!\n");
#endif
        while (sailor->next)
          sailor = sailor->next;
        sailor->next =
            (struct usm_watch_list *)malloc(sizeof(struct usm_watch_list));
        sailor = sailor->next;
        sailor->com =
            (unsigned int *)mmap(NULL, SYS_PAGE_SIZE, PROT_READ | PROT_WRITE,
                                 /*MAP_FIXED |*/ MAP_SHARED, usmSleepersFd, 0);
        if (sailor->com ==
            MAP_FAILED) { // NULL it up before..? Malloc intricacies..
#ifdef DEBUG
          printf("[Mayday] Mapping failed! Aborting!");
#endif
          // close..?
          abort(); // no abort.. graciously spill it..
        }
      sync:
        sailor->pid = *(int *)((uintptr_t)sailor->com + 2048);
        if (!sailor->pid)
          goto sync;
#ifdef DEBUG
        printf("[Info] Lil' new boi's pid : %d\n", sailor->pid);
#endif

#ifdef UINTR
        sailor->pid = syscall(SYS_pidfd_open, sailor->pid,
                              0); // write it at some predefined zoney..
        if (sailor->pid == -1) {
          perror("pidfd_open failed ");
          abort(); // no abort.. graciously spill it.
        }
#ifdef TSERV
        sailor->timer = syscall(
            SYS_pidfd_getfd, sailor->pid,
            *(int *)((uintptr_t)sailor->com + 2048 + sizeof(int) * 2), 0);
        if (sailor->timer == -1) {
          perror("pidfd_getfd failed ");
          abort(); // no abort.. graciously spill it..
        }
#endif
        sailor->pid =
            syscall(SYS_pidfd_getfd, sailor->pid,
                    *(int *)((uintptr_t)sailor->com + 2048 + sizeof(int)), 0);
        if (sailor->pid == -1) {
          perror("pidfd_getfd failed ");
          abort(); // no abort.. graciously spill it..
        }
#ifdef TSERV
        if (uintr_register_sender(sailor->timer, 0) < 0) {
          perror("reg_send_uipi, 1 ");
          abort(); // no abort.. graciously spill it..
        }
#endif
        if (uintr_register_sender(sailor->pid, 0) < 0) {
          perror("reg_send_uipi, 0 ");
          abort(); // no abort.. graciously spill it..
        }
        __WRITE_ONCE(*(unsigned int *)((uintptr_t)(sailor->com)), 0);
#endif
        sailor->state = (bool *)((uintptr_t)sailor->com + 4092);
      } else {
        if (likely(pollfd[0].revents == POLLPRI)) {
#ifdef DEBUG
          printf("Yo! Unreggin' some distant sleeper!\n");
#endif
          struct usm_watch_list *tempList = &usmMemWatchList;
          while (tempList) {
            struct usm_watch_list *next = tempList->next;
            if (unlikely(
                    !next &&
                    tempList ==
                        &usmMemWatchList)) { // should we even scan everything..
                                             // to see later...
#ifdef DEBUG
              printf("No distant sleeper uo! Implicit POLLERR..\n");
#endif
              munmap(tempList->com, SYS_PAGE_SIZE);
              abort();
            }
            if (*(int *)((uintptr_t)(next->com) + 3000) !=
                2) // TODO maybe do it all the time with some unlikely.. at
                   // least it'll be more stable...      like, it should be
                   // faster (faults' treatment) as is, but looking across
                   // everyone when booming.. Idk, TODO investigate
              goto nextBeem;
            // tempList->next = next->next;     // .. whole purpose of all these
            // intricacies.. TODO use dis maybe.., for now replaced by..
            tempList =
                tempList->next; // ^ to avoid passing by the already treated
                                // next a n d a potentiel segfault..
            // void *toUnmap = next->com;
            // next->com = NULL;        .. now simply freed     .. nope anym.,
            // down.. in the locks trip..
            munmap(next->com, SYS_PAGE_SIZE);
            // free(next);
            next->com = NULL;
            continue;
          nextBeem:
            tempList = tempList->next;
          }
        } else {
          if (pollfd[0].revents == POLLERR) {
#ifdef DEBUG
            printf("No one up anymore! Ciaossu!\n");
#endif
            abort();
          }
#ifdef DEBUG
          printf("Unrecognized poll val., aborting!\n");
#endif
          abort();
        }
      }
    }
  }
}
/*
if (uintr_register_sender(vcpu_list[wk->id].upid_fd, 0) < 0)
        exit(1);
*/

int main(int argc, char *argv[]) {
  int usmSleepersFd = open("/proc/usmSleepers", O_RDWR);
  struct pollfd pollfd[1];
  pollfd[0].fd = usmSleepersFd;
  pollfd[0].events = POLLIN;
  pthread_t mapperThread;
#ifdef TSERV
  char *endptr;
  if (argc < 2) {
    printf("[Sys] Compiled with time server dropback support.. please specify "
           "ticks' amount\n");
    exit(1);
  }
  tme = strtoll(argv[1], &endptr, 10);
  if (endptr == argv[1]) {
    printf("[Sys/TServ] Please give actual numbers\n");
    exit(1);
  } else if (*endptr != '\0') {
    printf("[Sys/TServ] Invalid character: %c\n", *endptr);
    exit(1);
  }
#ifdef DEBUG
  else {
    printf("[Sys/TServ] Timer: %ld\n", tme); // %lld..
  }
#endif
#endif
  if (usmSleepersFd < 0) {
    printf("[Mayday] A waker's already up!\n");
    abort();
  }
#ifdef DEBUG
  else
    printf("[Info] usmSleepersFd : %d\n", usmSleepersFd);
#endif
  int ret = poll(pollfd, 1, -1);
#ifdef DEBUG
  printf("[Info] Sum... %d, %d\n", ret, pollfd[0].revents);
  /*ret = poll(pollfd, 1, -1);
  printf("[Info] Sum... %d, %d\n", ret, pollfd[0].revents);*/
  ret = poll(pollfd, 1, 0);
  printf("[Info] Sum... %d, %d\n", ret, pollfd[0].revents);
  ret = poll(pollfd, 1, 5);
  printf("[Info] Sum.... %d, %d\n", ret, pollfd[0].revents);
  if (ret <= 0) {
    printf("[Info] No one's up anymore..! Ciaossu!\n");
    abort();
  } else {
    printf("[Info] Sum. bad boi up but.. %d, %d\n", ret, pollfd[0].revents);
  }
  if (pollfd[0].revents < 0) {
    printf("[Info] Yeesh...\n");
  }
  printf("Yo! Reggin' first distant sleeper!\n");
#endif
  printf("[Yee/%d] Boi's in town! Prepare to be super duper up!\t(ye.. exec "
         "dis.)\n",
         usmSleepersFd);
  (&usmMemWatchList)->com =
      (unsigned int *)mmap(NULL, SYS_PAGE_SIZE, PROT_READ | PROT_WRITE,
                           /*MAP_FIXED |*/ MAP_SHARED, usmSleepersFd, 0);
  if ((&usmMemWatchList)->com ==
      MAP_FAILED) { // NULL it up before..? Malloc intricacies..
    printf("[Mayday] Mapping failed! Aborting!");
    getchar();
    // close..?
    abort();
  }
fsync:
  (&usmMemWatchList)->pid =
      *(int *)((uintptr_t)((&usmMemWatchList)->com) + 2048);
  if (!(&usmMemWatchList)->pid)
    goto fsync;

#ifdef DEBUG
  printf("[Info] Lil' boi's pid : %d\n", (&usmMemWatchList)->pid);
#endif

#ifdef UINTR
  (&usmMemWatchList)->pid = syscall(SYS_pidfd_open, (&usmMemWatchList)->pid,
                                    0); // write it at some predefined zoney..
  if ((&usmMemWatchList)->pid == -1) {
    perror("pidfd_open failed ");
    abort();
  }
#ifdef TSERV
  (&usmMemWatchList)->timer = syscall(
      SYS_pidfd_getfd, (&usmMemWatchList)->pid,
      *(int *)((uintptr_t)(&usmMemWatchList)->com + 2048 + sizeof(int) * 2), 0);
  if ((&usmMemWatchList)->timer == -1) {
    perror("pidfd_getfd failed ");
    abort();
  }
#endif
  (&usmMemWatchList)->pid = syscall(
      SYS_pidfd_getfd, (&usmMemWatchList)->pid,
      *(int *)((uintptr_t)(&usmMemWatchList)->com + 2048 + sizeof(int)), 0);
  if ((&usmMemWatchList)->pid == -1) {
    perror("pidfd_getfd failed ");
    abort();
  }
#ifdef TSERV
  if (uintr_register_sender((&usmMemWatchList)->timer, 0) < 0) {
    perror("reg_send_uipi, 1 ");
    abort(); // no abort.. graciously spill it..
  }
#endif
  if (uintr_register_sender((&usmMemWatchList)->pid, 0) < 0) {
    perror("reg_send_uipi, 0 ");
    abort(); // no abort.. graciously spill it..
  }
  __WRITE_ONCE(*(unsigned int *)((uintptr_t)((&usmMemWatchList)->com)),
               0); // dangerous.. TODO probe it right..
#endif

  (&usmMemWatchList)->state =
      (bool *)((uintptr_t)((&usmMemWatchList)->com) + 4092);
  (&usmMemWatchList)->next = NULL;
#ifdef DEBUG
  printf("[Info] Lil' boi's state : %d\n", *((&usmMemWatchList)->state));
  printf(
      "[Info] We'll care 'bout 'em mapper later. -_-. (yup, now)\n"); // pthread_create(&mapperThread,
                                                                      // NULL,
                                                                      // mapper,
                                                                      // (void*)&usmSleepersFd);
  int l = 0;
#endif
  while (1) {
    struct usm_watch_list *tempList = &usmMemWatchList;
    /*
    if(unlikely((ret = poll(pollfd, 1, 0)))) {                  // let a mapper
thread do this? But the locks...? But the timer's timings' falsing possibilites
as is....?  TODO, base, IMP. if(ret < 0) { #ifdef DEBUG printf("No one's up
anymore..! Ciaossu!"); #endif exit(EXIT_SUCCESS);
        }
        struct usm_watch_list *sailor = &usmMemWatchList;       // could
statefully preserve last boi but meh, think that out booming first before any
even dope thing. if (pollfd[0].revents == POLLIN) { #ifdef DEBUG printf("Yo!
Reggin' some distant sleeper!\n"); #endif while(sailor->next)
                sailor=sailor->next;
            sailor->next=(struct usm_watch_list *)malloc(sizeof(struct
usm_watch_list)); sailor=sailor->next; sailor->com=(unsigned int *)mmap(NULL,
SYS_PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, usmSleepersFd, 0);
            if(sailor->com == MAP_FAILED) {    // NULL it up before..? Malloc
intricacies.. #ifdef DEBUG printf("[Mayday] Mapping failed! Aborting!"); #endif
                // close..?
                abort();    // no abort.. graciously spill it..
            }
sync:
            sailor->pid=*(int *)((uintptr_t)sailor->com+2048);
            if (!sailor->pid)
                goto sync;
#ifdef DEBUG
            printf("[Info] Lil' new boi's pid : %d\n", sailor->pid);
#endif

#ifdef UINTR
            sailor->pid=syscall(SYS_pidfd_open, sailor->pid, 0);     // write it
at some predefined zoney.. if (sailor->pid == -1) { perror("pidfd_open failed
"); abort();    // no abort.. graciously spill it.
            }
            sailor->timer=syscall(SYS_pidfd_getfd, sailor->pid, *(int
*)((uintptr_t)sailor->com+2048+sizeof(int)*2), 0); if (sailor->timer == -1) {
                perror("pidfd_getfd failed ");
                abort();    // no abort.. graciously spill it..
            }
            sailor->pid=syscall(SYS_pidfd_getfd, sailor->pid, *(int
*)((uintptr_t)sailor->com+2048+sizeof(int)), 0); if (sailor->pid == -1) {
                perror("pidfd_getfd failed ");
                abort();    // no abort.. graciously spill it..
            }
            if (uintr_register_sender(sailor->timer, 0) < 0) {
                perror("reg_send_uipi, 1 ");
                abort();    // no abort.. graciously spill it..
            }
            if (uintr_register_sender(sailor->pid, 0) < 0) {
                perror("reg_send_uipi, 0 ");
                abort();    // no abort.. graciously spill it..
            }
            __WRITE_ONCE(*(unsigned int *)((uintptr_t)(sailor->com)), 0);
#endif
            sailor->state=(bool *)((uintptr_t)sailor->com+4092);
        }
        else {
            if(likely(pollfd[0].revents == POLLPRI)) {
#ifdef DEBUG
                printf("Yo! Unreggin' some distant sleeper!\n");
#endif
                struct usm_watch_list *tempList = &usmMemWatchList;
                while (tempList) {
                    struct usm_watch_list *next = tempList->next;
                    if(unlikely(!next && tempList == &usmMemWatchList)) { //
should we even scan everything.. to see later... #ifdef DEBUG printf("No distant
sleeper uo! Implicit POLLERR..\n"); #endif munmap(tempList->com, SYS_PAGE_SIZE);
                        abort();
                    }
                    if(*(int *)((uintptr_t)(next->com)+3000) != 2)      // TODO
maybe do it all the time with some unlikely.. at least it'll be more stable...
like, it should be faster (faults' treatment) as is, but looking across everyone
when booming.. Idk, TODO investigate goto nextBeem;
                    // tempList->next = next->next;     // .. whole purpose of
all these intricacies.. TODO use dis maybe.., for now replaced by..
                    tempList=tempList->next;        // ^ to avoid passing by the
already treated next a n d a potentiel segfault..
                    // void *toUnmap = next->com;
                    // next->com = NULL;        .. now simply freed     .. nope
anym., down.. in the locks trip.. munmap(next->com, SYS_PAGE_SIZE);
                    // free(next);
                    next->com = NULL;
                    continue;
nextBeem:
                    tempList=tempList->next;
                }
            }
            else {
                if(pollfd[0].revents == POLLERR) {
#ifdef DEBUG
                    printf("No one up anymore! Ciaossu!\n");
#endif
                    abort();
                }
#ifdef DEBUG
                printf("Unrecognized poll val., aborting!\n");
#endif
                abort();
            }
        }
    }*/
#ifdef DEBUG
    int prt = 0;
#endif
    while (tempList) {

      // if(unlikely(!tempList->com)) {
      /*struct usm_watch_list *tempDel = tempList;
      tempList=tempList->next;
      free(tempDel);*/        // haha..
      /*struct usm_watch_list *tempPList = &usmMemWatchList;
      while (tempPList) {
          struct usm_watch_list *next = tempPList->next;*/
      /*if(unlikely(!next && tempPList == &usmMemWatchList)) {       // should
we even scan everything.. to see later...      // done by the mapper.. ; and,
either way, we should just make some prev. ver. man.. #ifdef DEBUG printf("No
distant sleeper uo! Implicit POLLERR..\n"); #endif munmap(tempList->com,
SYS_PAGE_SIZE); abort();
      }*/
      /*if(tempPList->next == tempList) {
          tempPList->next = tempList->next;
          break;
      }
  }
  free(tempList);
  continue;
}*/

      if (__READ_ONCE(*(tempList->state))) {
        // watch the timers then beem it out if necessary..
#ifdef DEBUG
        if (prt++ % 100000000 == 0)
          printf("...bravo group %d!\t%d\n", tempList->pid, *tempList->state);
#endif
#ifdef TSERV
        if (dpreempt(tempList->com + 3000 +
                     sizeof(int) *
                         2)) // Debug messages should not be plausible..
#ifdef UINTR
          _senduipi(1); // not.. this.... tempList->timer.... and move it to
                        // make it 1, TODO
#else
          kill(tempList->pid, SIGUSR2);
        ; // put here signals' version..
#endif
#endif
        goto next;
      }
      if (__READ_ONCE(*tempList->com)) {
#ifdef DEBUG // ain't super necessary..
        printf("Man.. tempList->pid : %d\t%d\n", tempList->pid, l++);
#endif
#ifdef UINTR
        _senduipi(0); // not.. dis... shooty shoot man, freak's sake! ...
                      // tempList->pid
#else
        kill(tempList->pid, SIGUSR1); // pthread_...
#endif
        // __WRITE_ONCE(*tempList->state, true);        // heavification of the
        // other side.. but bug solving testing... | hmm.. leg.
#ifdef DEBUG
        printf("[maydayOne] Triggered vCPU** bravo group %d!\t%d\n",
               tempList->pid, *tempList->state); // ofc, some %s on bravoOne..
#endif
      }
    next:
      tempList = tempList->next;
    }
    // sleep(1);
  }
}
