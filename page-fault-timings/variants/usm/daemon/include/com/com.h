/*
    - Le noyau et usm communiquent via des canaux.
    - usm crée des canaux au démarrage.
    - un canal a un type. Nous avons identifié des canaux file based
   (userfaultfd et procfs), RAM based (iouring), hybrid (uio drivers)
    - un canal est surveillé ou gérer par un worker. L'implantation de ce
   dernier dépend évidemment du type de canal.
    - un worker peut gérer plusieurs canaux de même type
    - les canaux transportent des événements
    - un canal a un sens de transport d'événements (user->kernel ou
   kernel->user). Un canal a d'autre propriété: priorité par rapport aux autres
   canaux, une taille, il ordonne ou pas les événements.
    - un événement a un type, un niveau de priorité, et d'autres paramètres
    - un worker est poll, périodique
*/
#ifndef COM_H
#define COM_H
#include "event.h"
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

#include "../usm/utils.h" // might'n't be necessary.. (indirection)
#include <ucontext.h>

// #define MAX_EVT 100
#define STACK_SIZE 131072
#define MAX_UTHREADS sizeof(int) * 8
#define MAX_IUTHREADS sizeof(int) * 8
#define MAX_VCPUS 64 // 10   but 100 kernel wise ftm

/*
    Définition des canaux
*/
enum usm_channel_type { FD_BASED, RAM_BASED, HYBRID };
enum usm_direction_type { USER2KERNEL, KERNEL2USER };
struct usm_channel_ops {
  int (*usm_init)();  // instantiate a poll_fd thingy?
  int (*usm_clean)(); // mode ?
  int (*usm_check)(int channel_id, int timeout);
  int (*usm_check_mem)(char *mem, int timeout);
  int (*usm_retrieve_evt)(struct usm_event *event);
  int (*usm_retrieve_evts)();
  int (*usm_submit_evt)(struct usm_event *event);
  int (*usm_submit_evts)();
  int (*usm_is_full)();
};

typedef struct usm_channel {
  int usm_cnl_id; // char..                   // UTs : temporary reference to
                  // wk_id..
  int fd;         // char *file_name;//pour les fd_based
  // char *buff;//pour les ram_based              // duplicate of event's inner
  // thingies now..
  int usm_cnl_prio; // priority within the group
  enum usm_channel_type usm_cnl_type;
  enum usm_direction_type usm_drn_type;
  struct usm_channel_ops *usm_cnl_ops;
  struct usm_event event;
  struct list_head iulist;
  int (*usm_handle_evt)(void *channel);
  // struct usm_channel* next;
} usm_channel;

/*typedef struct usm_channels_node {
    struct usm_channel *usm_channel;
    struct list_head iulist;
} usm_channels_node;*/

struct manIU_t {
  void *currentiUT; // vUthread_t *currentiUT;
  unsigned int tempIUTEvents;
  unsigned int *iUTcom;
  int ioctl_fd;    // temp. .. maybe...
  void *iuthreads; // vUthread_t iuthreads[MAX_IUTHREADS];		// kinda
                   // "redundantly" weird..?			|Ye.. no
                   // usm_event : simply ucontexts.. hence preemptable.
};
/* Ye.. usm_event still way too big.. TODO unionify it */
union uthread_data {
  struct manIU_t manager;
  struct usm_event event;
};

typedef struct {
  ucontext_t task;
  // ucontext_t spin;
  int id;
  char task_stack[STACK_SIZE];
  // char spin_stack[STACK_SIZE];
  // struct usm_worker wk_args;
  int state;

  /* TODO mix in sum union thingy.. */
  // int type;			// temp.ly int...	| and temp. ...
  // | ye no real need..
  union uthread_data alt;
  // int fd;			// only atm used by spec. UT treatment.. hence
  // temp. place, but possibly not..
} uthread_t;

typedef struct {
  bool *state;
  pthread_t thread; // star'ize TODO or merge with wk_args..
  uthread_t uthreads[MAX_UTHREADS];
  uthread_t *current_uthread;
  int upid_fd;
  int cpu_affinity;
  struct usm_worker *wk_args;
  unsigned int tempEvents;
  ucontext_t uctx_main;
} vcpu_t;

extern pthread_mutex_t managersLock;
extern unsigned int nb_vcpus;
extern void *soldiers(void *wk_args);
extern vcpu_t vcpu_list[MAX_VCPUS];

/*
    Définition des workers
*/
enum usm_worker_type { POLL, PERIODIC }; // 'kay.. TBD
struct usm_worker_ops {
  int (*usm_init)();
  int (*usm_clean)();
  int (*usm_start)();
  int (*usm_sort_evts)(); // trie les evts
  int (*usm_stop)();
  int (*usm_handle_evt)(struct usm_event *event); // traite l'evt
};
extern struct usm_worker {
  char *thread_name;
  pthread_t thread_id;
  // int usm_period;
  // int usm_budget;
  // struct usm_event usm_current_events[MAX_EVT]; // c'est ici que le worker
  // place les evts des canaux qu'il gère lorsqu'il fait un tour de check struct
  // list_head usm_current_events; int usm_current_events_length;   Could have
  // some fun feeding this.. but wow, imagining the overhead (although some easy
  // calculations doable on that bitfield.. and, we'll be saying 64's max...(!))
  usm_channel
      usm_channels[MAX_UTHREADS]; // 64.. by usmid... // struct list_head
                                  // usm_channels;       // actual UTHREADs..
  unsigned int *com; // char*! Temp.     | unsigned long long.. or smthn
                     // better.. temp.! TODO
  // pthread_mutex_t chnmutex;
  pthread_attr_t attrs;
  struct usm_worker_ops *usm_wk_ops;
  struct usm_worker *next; // maybe do the same...
  // uint8_t vcpu_id;     no even need here.
  int id; // int managedCount;       // if 64.. temp.     // now told by
          // kernel..
  bool *state;
} usmWorkers;

extern struct usm_worker usmMemWorkers;

extern struct usm_worker usmMemWorkersWatch;
extern struct usm_watch_list {
  unsigned int
      *com; // char*! Temp. ... and should we just give out usm_worker..?
  pthread_t *thread_id;
  bool *state;
  struct usm_watch_list *next;
} usmMemWatchList;

// extern struct usm_channel_ops usm_cnl_userfaultfd_ops, usm_cnl_freed_ops,
// usm_cnl_new_process_ops;
extern struct usm_channel usm_cnl_userfaultfd, usm_cnl_freed_pgs, usm_cnl_nproc;
extern struct usm_worker usm_wk_uffd, usm_wk_free_pgs, usm_wk_nproc;

extern struct usm_channel_ops usm_cnl_userfaultfd_ops;

extern struct usm_channel_ops usm_cnl_mem_ops;

extern struct usm_channel_ops usm_cnl_emul_ops;

extern struct usm_channel_ops usm_cnl_special_ops;

extern struct usm_channel_ops usm_cnl_freed_ops;

extern struct usm_channel_ops usm_cnl_new_process_ops;

extern void *usm_handle_evt_poll(void *wk_args);
extern void *usm_mem_worker_handler(void *wk_args);
extern void *usm_mem_workers_waker(void *wk_args);
extern void *usm_handle_evt_periodic(void *wk_args);

extern int usm_handle_mem(struct usm_channel *channel);
extern int usm_handle_mem_special(struct usm_event *event);
extern void *usm_handle_fd(void *arg);

/* Temp.. prol'ly ; even maybe just fuse 'em up into one and goto around... */
extern void usm_mem_worker_handler_singular(int wk_args);
extern void usm_mem_worker_handler_uts(int wk_args);
#endif