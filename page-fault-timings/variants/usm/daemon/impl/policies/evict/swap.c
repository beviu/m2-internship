#include "../../../include/usm/usm.h"

struct usm_swap_policy_ops default_swap_policy;

void (*usm_evict_fallback)(struct usm_event *event);
void (*do_cond_swap_in)(struct usm_event *event);
void (*do_cond_swap_out)(struct usm_event *event);

struct usm_swap_dev *(*choose_swap_device)(
    int proc /*struct usm_event * event*/);
struct usm_swap_dev *(*deviceNumberToSwapDevice)(int n);
// int (*usmSwapDeviceToNumber) (struct usm_swap_dev * swapDev);

void usmAddProcSwapNode(struct to_swap *toSwap) { // put to_swap in event man!
  pthread_mutex_lock(
      &procDescList[toSwap->proc].swpLock); // per node..., once again..
  list_add(&toSwap->iulist, &procDescList[toSwap->proc].swapList); // __
  pthread_mutex_unlock(&procDescList[toSwap->proc].swpLock);
}

/*void usmDeleteProcSwapNode(struct to_swap * toSwap) {           // put to_swap
in event man! pthread_mutex_lock(&procDescList[toSwap->proc].swpLock);
    list_del(&toSwap->iulist);
    pthread_mutex_unlock(&procDescList[toSwap->proc].swpLock);
    free(toSwap);
}*/

void usmHoldProcSwapNode(struct to_swap *toSwap) { // put to_swap in event man!
  pthread_mutex_lock(&procDescList[toSwap->proc].swpLock);
  list_del(&toSwap->iulist);
  pthread_mutex_unlock(&procDescList[toSwap->proc].swpLock);
}

int trySwapCache(struct usm_event *event) {
  if (!list_empty(&procDescList[event->origin].swapCache)) {
    struct to_swap *swapCacheIterator;
    list_for_each_entry(swapCacheIterator,
                        &procDescList[event->origin].swapCache, iulist) {
      if (swapCacheIterator->swapped_address == event->vaddr) {
        swapCacheIterator->retaken = 1;
        event->paddr = swapCacheIterator->page->physicalAddress;
        usmSubmitAllocEvent(event);
        return 1; // some locks needed!
      }
    }
  }
  return 0;
}

int init_swap_device_nodes(
    struct usm_swap_dev *swap_dev,
    unsigned long
        size) { // maybe some fancy (-_-') ops struct or func.s passing.. -_-
  INIT_LIST_HEAD(&swap_dev->swap_list);
  INIT_LIST_HEAD(&swap_dev->free_list);
  if (size % SYS_PAGE_SIZE || size > 1UL << 8 * sizeof(unsigned long) - 3)
    return 1;
  swap_dev->swap_nodes = (struct swap_node *)malloc(((size / SYS_PAGE_SIZE)) *
                                                    sizeof(struct swap_node));
  if (unlikely(!swap_dev->swap_nodes))
    return 1;
  unsigned long itr = 0;
  while (itr * SYS_PAGE_SIZE < size) {
    struct swap_node *snode = swap_dev->swap_nodes + itr;
    snode->offset = itr * SYS_PAGE_SIZE;
    INIT_LIST_HEAD(&(snode->iulist));
    list_add(&snode->iulist, &swap_dev->free_list);
    itr++;
  }
#ifdef SpDEBUG
  printf("Maxi. offset : %lu\n", itr * SYS_PAGE_SIZE);
#endif
  pthread_mutex_init(&swap_dev->devListLock, NULL);
  pthread_mutex_init(&swap_dev->devSwapSpaceLock, NULL);
  return 0;
}