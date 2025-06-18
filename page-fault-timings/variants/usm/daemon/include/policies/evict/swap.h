#include "../alloc/alloc.h"
#include "swap_dev.h"
/*
        - Toute politique de swapping dans USM doit implémenter cette interface.
        - Le runtime d'USM appellera ces fonctions
        - Pour l'instant l'on ne connait pas encore les types de retours et les
   paramètres.
        - La liste évidemment n'est pas exhaustive
*/

#define SWAP_NR 3 // some fancy shifting or in another #define.. or just -_-'..

extern struct usm_swap_policy_ops {
  char *usm_swap_policy_name;
  int (*usm_init)(); // initialise la politique. Par exemple les structures de
                     // données de la politique peuvent être initialisées dans
                     // cette fonction.
  int (*usm_swap)(struct usm_event *event); // swappe in de la mémoire physique
  void (*cond_swap_out)(struct usm_event *event); // union in event of to_swap
  // int (*do_cond_swap_in) (struct usm_event *event);       // thread routine?
  // Us to launch thingies.. or him? Should we just provide routines..?
  // usm_swap_out_routine/usm_launch_swap_out    and he provides the lock of his
  // lists, the format (type) of his structs (we force him to use lx's list)..
  // and so on.. but.. container(S) inside container..? Rah man...     nah,
  // usm_fill_swap_list with a new list he'll have made.. yes!
  // usm_create_swap_list -> returns list_head, usm_fill.. with normalized
  // types.. and in the launch we destroy.. yeah, so swap.h in USM has something
  // relevant now... but then, where to launch it all -_-'         his
  // do_cond_swap JUST checks on W H A T E V E R he wants.. we'll provide
  // helpers.. as in getAlloctedProcess.. getGlobalAllocated (he has it through
  // usmMem..).. getActiveProcessList.. but with his to_swap he already has it
  // all.. "proc..".. so getProcName.. then see in his list if there was
  // something at the left and/or right... ... or he just booms his global
  // list.. yes, this should do!         but the lock.. hmm.. yes just one for
  // now, per process should comply...! Now, that "doubly" linked list.. in the
  // sense that one head booms to everything, and another head greps all the
  // other heads.. so u get directly all active proc.s.. with something a bit
  // clever....hmm..
  //  ^-> essentially usm_swap/*_in*/!
  int (*usm_free)(
      struct usm_event *event); // libère l'espace de swapping associé
  int (*usm_hints)(); // la politique reçoit de nouveaux hints de l'adminsys
  int (*usm_virt_address_space_change)(); // la politique peut avoir besoin de
                                          // savoir qu'il y a eu un changement
                                          // dans l'espace d'adressage virtuel
                                          // des processus
  // un exemple de changement est le changement de taille d'une VMA.
  int (*usm_phys_address_space_change)(); // la politique peut avoir besoin de
                                          // savoir si pour une raison ou une
                                          // autre un mapping physique a été
                                          // défait par un autre composant, par
                                          // exemple le swapper, le OOM.
  int (*usm_permissions_change)(); // changement sur les permissions des VMAs
  int (
      *usm_process_state_change)(); // le changement de l'état du processus peut
                                    // influencer la politique d'allocation
  int (*usm_new_process)(); // un nouveau processus a été associé à la politique
} default_swap_policy;

extern void (*usm_evict_fallback)(struct usm_event *event);
extern void (*do_cond_swap_in)(struct usm_event *event);
extern void (*do_cond_swap_out)(struct usm_event *event);

extern struct evict_worker { //.. seems quite lame to have a "worker" not be
                             //with the _others_       TODO
  char thread_name[16];
  pthread_t thread_id;
  struct evict_worker *next;
} evictWorkers;

struct to_swap { // union.. | toOptimize
  struct usm_swap_dev
      *swapDevice; // hinders swap_in's perf.&Co., as optimized until now... (if
                   // one bit taken, limits directly takable swap space to
                   // 8192Go.. which still is quite huge though :'D... now would
                   // one bit suffice... n o. Two bits -> 4096 limit, 3 swap
                   // devices... three bits, 2048 Go limit, 7 swap devices...
                   // which should be a good limit.. furthermore, the other
                   // PTE's slots, except ones pertaining to its' presence,
                   // could be used)
  struct page *page;
  unsigned long swapped_address;
  struct swap_node *snode; // off_t offset;
  int proc;                // should be unneeeded ; temporary.
  short retaken;           // bool
  struct list_head globlist;
  struct list_head iulist;
};

/* static inline struct to_swap * event_to_swap_entry (struct usm_event * event)
{ struct to_swap * toSwap = (struct to_swap *) malloc (sizeof(struct to_swap));
    toSwap->offset=event->offst;
    toSwap->proc=event->origin;
    toSwap->swapped_address=event->vaddr;
    return toSwap;
} legacy  */

/*#define __pteval_swp_type(x) ((unsigned long)((x).pte >> (64 -
SWP_TYPE_BITS))) #define __pteval_swp_offset(x) ((unsigned long)(~((x).pte) <<
SWP_TYPE_BITS >> SWP_OFFSET_SHIFT))

#define __pte_to_swp_entry(pte)	(__swp_entry(__pteval_swp_type(pte), \
                                             __pteval_swp_offset(pte)))*/

static inline struct to_swap *event_to_swap_entry(struct usm_event *event) {
  // printf("\nDevice number : %d\n", event->offst >> (sizeof(unsigned
  // long)*8-5)); if (!((deviceNumberToSwapDevice(event->offst >>
  // (sizeof(unsigned long)*8-5))->swap_nodes)+(event->offst << 5 >>
  // (5+9)))->toSwapHolder) printf("Hmm..\n");
  // printf("Offset : %lu\n\n", event->offst << 5 >> (5+9));
  // printf("Idx.. : %lu\n\n", (event->offst << 5 >> (5+9))/4096);       //
  // #weird getchar();
  return ((deviceNumberToSwapDevice(event->offst >>
                                    (sizeof(unsigned long) * 8 - 5))
               ->swap_nodes) +
          ((event->offst << 5 >> (5 + 9)) / 4096))
      ->toSwapHolder; // (struct to_swap
                      // *)((deviceNumberToSwapDevice(event->offst >>
                      // sizeof(unsigned long)*8-3)->swap_nodes)+(unsigned
                      // long)(event->offst << 3))->toSwapHolder;
}

/*#define __swp_entry(type, offset) ((swp_entry_t) { \
        (~(unsigned long)(offset) << SWP_OFFSET_SHIFT >> SWP_TYPE_BITS) \
        | ((unsigned long)(type) << (64-SWP_TYPE_BITS)) })*/

static inline unsigned long swap_value(
    struct to_swap
        *victimNode) { // sizeof(unsigned long)*8  ... i s n o t 6 4 | i S .
  /*printf("\n[S'OUT] Device number : %d\n", ~(~victimNode->snode->offset <<
  (9+5) >> 5 | ((unsigned long)victimNode->swapDevice->number<<(64-5))) >>
  (sizeof(unsigned long)*8-5)); printf("\n[S'OUT] Device number : %d\n",
  ~((~victimNode->snode->offset << (9+5) >> 5 | ((unsigned
  long)victimNode->swapDevice->number<<(64-5))) >> (sizeof(unsigned
  long)*8-5))); printf("\n[S'OUT] Device number : %d\n",
  (victimNode->snode->offset << (9+5) >> 5 | ((unsigned
  long)victimNode->swapDevice->number<<(64-5))) >> (sizeof(unsigned long)*8-5));

  printf("[S'OUT] Offset base : %lu\n\n", victimNode->snode->offset);
  printf("[S'OUT] Offset : %lu\n\n", ((~victimNode->snode->offset << (9+5) >> 5
  | ((unsigned long)victimNode->swapDevice->number<<(64-5))) << 5 >> (5+9)));
  getchar();*/
  return (((victimNode->snode->offset / 4096) << (9 + 5) >> 5 |
           ((unsigned long)victimNode->swapDevice->number << (64 - 5))) &
          ~((unsigned long)1 << 0)) &
         ~((unsigned long)1 << 8); // victimNode->snode->offset+((unsigned
                                   // long)victimNode->swapDevice->number<<(sizeof(unsigned
                                   // long)*8-3));
} // let's just say it's 5 for a bit..

extern void usmAddProcSwapNode(struct to_swap *toSwap);
// extern void usmDeleteProcSwapNode(struct to_swap * toSwap);
extern void usmHoldProcSwapNode(struct to_swap *toSwap);
extern int trySwapCache(struct usm_event *event);

/*static inline struct list_head * usm_create_thread_swap_list () {
    return (struct list_head *) malloc (sizeof(struct list_head));
}

static inline void usm_add_thread_swap_list (struct list_head * newToSwap,
struct list_head * threadSwapList) { list_add(newToSwap,threadSwapList);
}

static inline void usm_commit_swap_list_out (struct list_head * threadSwapList)
{
    // pthread.. with our routine           | plus add to mustWaitOn list.. in
usm_free.
}

static inline void usm_commit_swap_list_in (struct list_head * threadSwapList) {
    // pthread.. with our routine
}

static inline void usm_destroy_swap_list_and_file (struct usm_event *
event/*struct list_head * threadSwapList, FILE * swapFile*//*) {        // hmm.. if there was a fork.. and parent dead.. cow on swapped out pages.... care -_-' | check on forked list and append whole list to kids'
    
}*/

/* void * then cast up... in commit thingy..*/
static inline struct list_head *usm_create_list() {
  return (struct list_head *)malloc(sizeof(struct list_head));
}

static inline void usm_add_list(struct list_head *newToSwap,
                                struct list_head *threadSwapList) {
  list_add(newToSwap, threadSwapList);
}

static inline void usm_commit_swap_list_out(struct list_head *threadSwapList) {
  // pthread.. with our routine           | plus add to mustWaitOn list.. in
  // usm_free.
}

static inline void usm_commit_swap_list_in(struct list_head *threadSwapList) {
  // pthread.. with our routine
}

// He'll just free it.. but sure, gotta be a parkour.... hmm, and the commit
// guy'll take care of it all so.... but he could want it, in like he got
// thingies but didn't like it all (he wanted X pages but got X-1 and decides to
// rip it all.. although in that case the put_back thingy should destroy it per
// se..)
/*static inline void usm_destroy_swap_list_and_file (struct usm_event *
 * event/*struct list_head * threadSwapList, FILE * swapFile*//*) {        // hmm.. if there was a fork.. and parent dead.. cow on swapped out pages.... care -_-' | check on forked list and append whole list to kids'
    
}*/