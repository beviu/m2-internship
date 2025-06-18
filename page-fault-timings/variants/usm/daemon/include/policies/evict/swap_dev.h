#include "../alloc/alloc.h"

struct to_swap;

struct usm_swap_dev_ops {
  // int (*init) (struct usm_swap_dev * swap_dev);    func. provided down below
  // int (*swap_out)(struct list_head * toSwap, void
  // (*put_back_used_list)(struct page * page));      no real need to pass said
  // func. as arg.
  int (*swap_out)(
      struct to_swap
          *toSwap); // (struct list_head * toSwap); (thingies already taking
                    // into account lists, in other forms..)  // return 1 if
                    // everything got swapped out, and 0 when there was some
                    // which didn't, with toSwap still containing those
  int (*swap_in)(struct to_swap *toSwap);
  // unsigned long
};

struct swap_node {
  // struct usm_swap_dev * swap_device;
  off_t offset;
  struct list_head iulist;
  struct to_swap *toSwapHolder; // some possible fancy container_of usage..
};

struct usm_swap_dev {
  short number;
  void *backend;
  struct list_head swap_list; // list of swapped out pages
  struct list_head free_list;
  struct usm_swap_dev_ops swap_dev_ops;
  struct swap_node *swap_nodes;
  pthread_mutex_t
      devListLock; // one per device ain't seem sufficiently efficient..
  pthread_mutex_t devSwapSpaceLock; // could exist devices who don't need it..
                                    // or devices who'd make use of more...
};

/*static inline struct swap_node * get_node(struct usm_swap_dev * dev) {
    struct swap_node * ret = NULL;
    pthread_mutex_lock(&dev->devListLock);
    if (likely(!list_empty(&dev->free_list))) {
        ret=list_first_entry(&dev->free_list, struct swap_node, iulist);
        list_move(&ret->iulist, &dev->swap_list);
    }
    pthread_mutex_unlock(&dev->devListLock);
    return ret;
}

static inline void put_node(struct swap_node * node) {
    pthread_mutex_lock(&node->swap_device->devListLock);
    list_move(&node->iulist, &node->swap_device->free_list);
    pthread_mutex_unlock(&node->swap_device->devListLock);
}*/

int init_swap_device_nodes(struct usm_swap_dev *swap_dev, unsigned long size);

extern struct usm_swap_dev *(*choose_swap_device)(
    int proc /*struct usm_event * event*/);
extern struct usm_swap_dev *(*deviceNumberToSwapDevice)(int n);
// extern int (*usmSwapDeviceToNumber) (struct usm_swap_dev * swapDev);
// basically unneeded..