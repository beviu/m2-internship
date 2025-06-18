#include <linux/anon_inodes.h>
#include <linux/bug.h>
#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/hashtable.h>
#include <linux/hugetlb.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mempolicy.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/mmu_notifier.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/security.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/userfaultfd_k.h>
#include <linux/wait.h>

#include <linux/pfn.h>
#include <linux/string.h>

#include <asm/pgtable.h>

MODULE_AUTHOR("None");
MODULE_DESCRIPTION("LKP USMRM");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.41");

#define DEBUG_USM 1

static const unsigned long usmMemSize = (unsigned long)512 * 1024 * 1024;

static struct proc_dir_entry *proc_entry;
static struct proc_dir_entry *proc_entry_sleepers;
static struct proc_dir_entry *proc_entry_christine_sleepers;
static struct proc_dir_entry *proc_entry_pgs;
// static int ver = 0;
// struct task_struct *toWake = NULL;

static dma_addr_t paddr;
static unsigned char *buf;
static volatile unsigned long index = 0;
static volatile unsigned long indext = 0;

extern wait_queue_head_t usm_wait; // static DECLARE_WAIT_QUEUE_HEAD(usm_wait);
// extern __poll_t sleepers_block_poll(struct file *file, poll_table *wait);
int waker_state = 0;
int mapped_sleepers = 0;
int mapped_vCPUs = 0; // per Christine instance TODO

unsigned long pgoff = 0;

extern struct list_head usm_sleepers_map_list; /* temp., mm.h*/
extern spinlock_t usm_sleepers_map_list_lock;

extern spinlock_t usm_list_lock;

extern int unmap_sleepers;

// LIST_HEAD(usm_christine_map_list);  /* temp., mm.h*/
// spinlock_t usm_christine_map_list_lock;

// static void *test;

// extern int usm_mt_mmap(struct file *filp, struct vm_area_struct *vma);

/*__poll_t usm_poll_sleepers(struct file *file, poll_table *wait)   // already
changed naught..
{
        return sleepers_block_poll(file,wait);
}*/

__poll_t usm_poll_sleepers(struct file *file, poll_table *wait) {
  // struct usm_ctx *usm_context = file->private_data;
  // struct usm_ctx *sailor = usm_list;
  // int tempCount = 1;      // mapped_sleepers;
  poll_wait(file, &usm_wait, wait);
  // usm_list = NULL;
  if (unlikely(!usm_list)) {
    printk(KERN_INFO "[Waker man.] NULL..!\n");
    return EPOLLERR;
  }

  /* Temp. management code */
  /*while(sailor->next) {
      sailor=sailor->next;
      tempCount++;
  }
  printk(KERN_INFO "[Waker man.] tempCount.. pre. : %d\n", tempCount);
  if(tempCount==1) {
      printk(KERN_INFO "[Waker man.] No one up..\n");
      return EPOLLERR;
  }
  printk(KERN_INFO "[Waker man.] tempCount.. : %d\n", tempCount);

  if(tempCount!=mapped_sleepers+1) {		// supplementary checks
  basically.. if(tempCount>mapped_sleepers+1) printk(KERN_INFO "[Waker man.]
  Multiple up to map! Next poll in some..\n"); else { printk(KERN_INFO "[Waker
  man.] Either bug.. or...\n"); if(tempCount<mapped_sleepers) printk(KERN_INFO
  "[Waker man.] Yup, one down... we gotta be able to POLLERR...\n");     // get
  a list to check up? With a munmap to do.. and a read/write to get the "ID" for
  waker to use.. yesh. return EPOLLERR;
      }
  }
  else {
      printk(KERN_INFO "[Waker man.] .. delivering guds.. %d\n", tempCount);
  }*/

  /*if(!list_empty(&usm_sleepers_unmap_list))   // just switch a global variable
     up and down.. simple. return POLLPRI;*/

  if (unmap_sleepers) {
    unmap_sleepers = 0;
    return EPOLLPRI;
  }

  // if !list_empty   usm_sleepers_map_list
  if (list_empty(&usm_sleepers_map_list))
    return 0;

  return EPOLLIN;
}

/*__poll_t usm_poll_christine(struct file *file, poll_table *wait)
{
        // struct christine_ctx *christine_context = file->private_data;
    int tempCount = 1;
    poll_wait(file, &usm_wait, wait);
    // usm_list = NULL;
    if (unlikely(!usm_list)) {
        printk(KERN_INFO "[Christine man.] No USM instance up..!\n");
        return EPOLLERR;
    }

    if(list_empty(&usm_christine_map_list))
        return 0;

    return EPOLLIN;
}*/

__poll_t usm_poll(struct file *file, poll_table *wait) {
  __poll_t ret = usm_poll_ufd(file, wait);
  // if (ret)
  // ver++;
  return ret;
}

static void usm_release(struct device *dev) {
  printk(KERN_INFO "Releasing USMd\n");
}

static struct device dev = {.release = usm_release};

static int usm_misc_f_release(struct inode *, struct file *) {
  printk(KERN_INFO "Releasing USMMcd\n");
  index = 0; // here... this, first, became pgoff. Then, get a structure
             // allocated at init, and put at one'a this task's unused zones..
             // there should be plenty enough.. and ..
  // walk it here, and free it.. and try to fuse with other ones.. or, even,
  // maybe (most probably not.. way too heavy), bring things some more back
  // together by copying (essentially moving out) thingies around same sized
  // zones, but contiguity blocking..
  // TODO
  return 0;
}

ssize_t usm_read(struct file *file, char __user *buf, size_t count,
                 loff_t *f_pos) {
  struct usm_ctx *usm_context = file->private_data;
  int fd = 0;
  // if (ver) {
  if (count < TASK_COMM_LEN) { // not quite needed..?
    printk(KERN_INFO "Size nah gud (%ld) #USMReadHandleUsmRM\n", count);
    goto outNope;
  }
  // ver--;
  fd = handle_usm(&usm_context->toWake);
  if (fd < 0)
    return -EFAULT;
  if (copy_to_user((__u64 __user *)buf, &usm_context->toWake->comm,
                   TASK_COMM_LEN)) {
    printk(KERN_INFO "[mayday/usm_read] Failed copy to user!\n");
    goto outNope;
  }

  return fd;
  //}
outNope:
  return -EAGAIN;
}

ssize_t usm_write(struct file *file, const char __user *buf, size_t count,
                  loff_t *f_pos) {
  struct usm_ctx *usm_context = file->private_data;
  if (unlikely(usm_context->toWake == NULL)) {
    printk(KERN_INFO "\t[mayday/usm_write] No process in queue!\n");
    return -EAGAIN;
  }
  wake_up_process(usm_context->toWake);
  return 0;
}

int usm_mmap(struct file *filp, struct vm_area_struct *vma) {
  // unsigned long page = virt_to_phys(buf+index) >> PAGE_SHIFT;
  unsigned long page =
      PFN_DOWN(virt_to_phys(phys_to_virt(paddr))) + vma->vm_pgoff +
      pgoff; // #define bus_to_virt phys_to_virt.. disappeared in 6.0.0+     //
             // vm_pgoff nonsensed here..    // lock.. pgoff... down here...
#ifdef DEBUG_USM
  printk("Base PFN : %lu", page);
#endif
  if (unlikely(index + (vma->vm_end - vma->vm_start) > usmMemSize)) {
    printk("Nope ma man, available : %luMB/%luMB ; at %lumb",
           usmMemSize / 1024 / 1024,
           (index + (vma->vm_end - vma->vm_start)) / 1024 / 1024,
           index / 1024 / 1024);
    return -ENOMEM;
  }
#ifdef DEBUG_USM
  printk(KERN_INFO "µ%lu\t%lu #USMRM\n", vma->vm_start, vma->vm_end);
#endif
  /*if (remap_pfn_range(vma, vma->vm_start, page, (vma->vm_end - vma->vm_start),
                      vma->vm_page_prot)) {*/
  if (remap_pfn_range(vma, vma->vm_start, page, (vma->vm_end - vma->vm_start),
                      vma->vm_page_prot)) {
    printk(KERN_INFO "[mayday/usm_mmap] remap failed..\n");
    return -1;
  }
  indext += vma->vm_end - vma->vm_start;
  pgoff = index / PAGE_SIZE;
#ifdef DEBUG_USM
  printk(KERN_INFO
         "remap_pfn_range ; cool, index : %ld. pgoff : %ld/%ldGB.\tTMPHCK\n",
         index, pgoff, index / 1024 / 1024 / 1024);
  printk(KERN_INFO "\tpgoff : %ld, | %ld.\n", vma->vm_pgoff,
         (unsigned long)vma->vm_pgoff + vma->vm_start - vma->vm_end);
#endif
  return 0;
}

int usm_mmap_sleepers(struct file *filp, struct vm_area_struct *vma) {
  // unsigned long page = virt_to_phys(buf+index) >> PAGE_SHIFT;
  // struct usm_ctx *sailor = usm_list;
  // int tempCount = mapped_sleepers;
  unsigned long page;
  // int toMapNb = 0;

  struct usm_ctx_p *usm_sl_cx;
  spin_lock(
      &usm_sleepers_map_list_lock); // could hamper perf.s if multiple threads
                                    // come up "simultaneously".. TODO lookItUp
  if (unlikely(list_empty(&usm_sleepers_map_list))) {
    spin_unlock(&usm_sleepers_map_list_lock);
    printk(KERN_INFO "[USM/waker/Mmap] Empty!\n");
    return -1;
  }
  usm_sl_cx = list_first_entry(&usm_sleepers_map_list, struct usm_ctx_p, ilist);
  list_del(&usm_sl_cx->ilist);
  spin_unlock(&usm_sleepers_map_list_lock);

  // ng:
  // while(/*tempCount-- && */sailor->next) {        // these two temporary..
  // get a simple list with exactly what to map instead of scurrying around to
  // find 'em.. sailor=sailor->next;
  //}
  // while(/*tempCount-- && */sailor->uthread[toMapNb++])
  //;
  // printk(KERN_INFO "[USM/Waker/Mmap] toMapNb : %d\n");    // , toMapNb-2);
  /*if(tempCount) {
      printk(KERN_INFO "[USM/Waker/Mmap] Not enough up! Waiting!\t%d\n",
  tempCount); if(tempCount<0) { printk(KERN_INFO "[USM/Waker/Mmap] Not even one
  up! Aborting!\t%d\n", tempCount); return -1;
      }
      while(!sailor->next)
          ;
      printk(KERN_INFO "[USM/Waker/Mmap] One more up! Move that binger down from
  usm_open.. (wake_up_poll).. TODO!\t%d\n", tempCount); goto ng;
      //  return -1;
  }*/
#ifdef DEBUG_USM
  printk(KERN_INFO "[USM/Waker/Mmap] Mapping sleeper %d!\n",
         mapped_sleepers + 1); // , toMapNb-2);
#endif
  page = PFN_DOWN(virt_to_phys((void *)usm_sl_cx->uctx)) +
         vma->vm_pgoff; // #define bus_to_virt phys_to_virt.. disappeared
                        // in 6.0.0+     // sailor->uthread[toMapNb-2]
#ifdef DEBUG_USM
  printk("Base sleeper's PFN : %lu", page);
  printk(KERN_INFO "[Sleeper]\tµ%lu\t%lu #USMRM\n", vma->vm_start, vma->vm_end);
#endif
  if (remap_pfn_range(vma, vma->vm_start, page, (vma->vm_end - vma->vm_start),
                      vma->vm_page_prot)) {
    printk(KERN_INFO "[mayday/Waker/usm_mmap] remap failed..\n");
    return -1;
  }
  mapped_sleepers++; // even dis guy's mostly debug.. or info.*..    *muchOne
#ifdef DEBUG_USM
  printk(KERN_INFO "remap_pfn_range - sleepers ; cool.\n");
  printk(KERN_INFO "\tpgoff : %ld, | %ld.\n", vma->vm_pgoff,
         (unsigned long)vma->vm_pgoff + vma->vm_end - vma->vm_start);
#endif

  kfree(usm_sl_cx);

  return 0;
}

int usm_mmap_christine(struct file *filp, struct vm_area_struct *vma) {
  struct page *page;

  /*struct usm_ctx_p *usm_sl_cx;
  //spin_lock(&usm_christine_map_list_lock);
  if (unlikely(list_empty(&usm_sleepers_map_list))) {
      spin_unlock(&usm_sleepers_map_list_lock);
      printk(KERN_INFO "[USM/Christine/Mmap] Empty!\n");
      return -1;
  }
      //usm_sl_cx = list_first_entry(&usm_christine_map_list, struct usm_ctx_p,
  ilist); list_del(&usm_sl_cx->ilist);
      // spin_unlock(&usm_christine_map_list_lock);*/
#ifdef DEBUG_USM
  printk(KERN_INFO "[USM/Christine/Mmap] Mapping vCPU %d!\n", mapped_vCPUs + 1);
#endif
  page = virt_to_page(
      (unsigned long)current
          ->usm_ctx); // PFN_DOWN(virt_to_phys(current->usm_ctx/*(void
                      // *)usm_sl_cx->uctx*/))+vma->vm_pgoff;	// #define
                      // bus_to_virt phys_to_virt.. disappeared in 6.0.0+
#ifdef DEBUG_USM
  printk(
      "Base Christine vCPU's PFN : %lu, val.s : %u, %u", page_to_pfn(page),
      *((unsigned int *)(current->usm_ctx + 4096 - sizeof(unsigned int) * 3)),
      *((unsigned int *)(current->usm_ctx + 4096 - sizeof(unsigned int) * 2)));
  printk(KERN_INFO "[Christine]\tµ%lu\t%lu #USMRM\n", vma->vm_start,
         vma->vm_end);
#endif
  if (remap_pfn_range(vma, vma->vm_start, page_to_pfn(page),
                      (vma->vm_end - vma->vm_start), vma->vm_page_prot)) {
    printk(KERN_INFO "[mayday/Christine/usm_mmap] remap failed..\n");
    return -1;
  }
  mapped_vCPUs++;
#ifdef DEBUG_USM
  printk(KERN_INFO "remap_pfn_range - Christine ; cool.\n");
  printk(KERN_INFO "\tpgoff : %ld, | %ld.\n", vma->vm_pgoff,
         (unsigned long)vma->vm_pgoff + vma->vm_end - vma->vm_start);
#endif
  // kfree(usm_sl_cx);

  return 0;
}

/* int usm_mt_mmap(struct file *filp, struct vm_area_struct *vma)
{
        struct page *page = NULL;
        struct usm_ctx_p *usmcx = list_first_entry(&usmmph, struct usm_ctx_p,
ilist);
        //WRITE_ONCE(*(int *)(usmcx->uctx), usmcx->usmid);
        printk(KERN_INFO ".. u on mem. #USM\n");   //, usmcx->usmid);
        // spin_lock(&ctx->usmlock);				// could hamper
perf.s if multiple threads come up "simultaneously".. TODO lookItUp        | now
in userspace.. but fork and pthread_create at the same time on different
threads.. hmm.. TODO, INV. list_del(&usmcx->ilist);
        // spin_unlock(&ctx->usmlock);

        printk(KERN_INFO "New thread mapping! #USMdAD\n");//, usmcx->usmid);
        //printk(KERN_INFO "pgoff | shifed #USM : %lu %lu\n", vma->vm_pgoff,
vma->vm_pgoff<<PAGE_SHIFT);

    page = virt_to_page((unsigned long)(usmcx->uctx));
//+(vma->vm_pgoff<<PAGE_SHIFT)); if (!page) { printk(KERN_INFO "Wut in the...
#USMAD\n"); return -1;
        }
    if (remap_pfn_range(vma, vma->vm_start, page_to_pfn(page), PAGE_SIZE,
                        vma->vm_page_prot)) {
        printk(KERN_INFO "[mayday/usm_mmap] remap_.. failed.. #faultsMmapAD\n");
                // spin_lock(&ctx->usmlock);          still userspacey..
                list_add(&usmcx->ilist, &usmmph);
                // spin_unlock(&ctx->usmlock);
        return -1;
    }
         // *(int *)(usmcx->uctx)=ctx->usmid;
        kfree(usmcx);
    //vma->vm_ops=&usm_ops;		// TODO further investigate
    return 0;
} */	// comm'ed out.. temporarily put back in for comp. purposes #newKernel...

/*int usm_mt_mmap_b(struct file *filp, struct vm_area_struct *vma)
{
    return usm_mt_mmap(filp, vma);
}*/

int usm_open(struct inode *ine, struct file *fl) {
  /*if (fl->f_flags!=O_RDWR) {
      printk(KERN_INFO "[USM] Linking to %d instance!\n", fl->f_flags);
      struct usm_ctx *sailor = usm_list;
      while(fl->f_flags-- && sailor->next) {
          sailor=sailor->next;
      }
      if(fl->f_flags)
          printk(KERN_INFO "[USM] Not enough up!\t%d\n", fl->f_flags);
      current->usm_x=(void *)sailor;
      return 1;   // to make it fail.. so no close needed afterwards
  }
  else
      printk(KERN_INFO "[USM] Normal open!\n");*/

  // here.. allocate!.. no, already done.. but meh... but meh : link it up here.
  // Do the syscall.. hmm.. no, the syscall's purely basic. Make the syscall, do
  // the mmap, put it in usm_x (or smthn), THEN call this to effectively link it
  // up. Simple

  if (fl->f_flags & O_RDWR) {
    fl->private_data = (struct usm_ctx *)current->usm_x; // TODO ver. dis.
    // wake_up_poll(&usm_wait, EPOLLIN);
  }
#ifdef DEBUG_USM
  else {
    printk(
        KERN_INFO
        "Nope man.. not changing that to oblivion!\n"); // though.. it should
                                                        // be... another file..
                                                        // completely..
                                                        // different..
                                                        // address.... damn.
    printk(KERN_INFO "But tell me tell me.. : %s\n",
           fl->private_data ? "TRUE/Friggin.' non NULL" : "False..NULL.");
  }
#endif
  /*if (!test) {
      test=(void *)fl;
      fl->private_data=(void *)1;
      printk(KERN_INFO "Armed!\n");
  }
  else {
      fl->private_data=(void *)2;
      if (test==fl) {
          printk(KERN_INFO "Same!\n");
          if(((struct file *)test)->private_data == fl->private_data)
              printk(KERN_INFO "Sadly same....\n");
      }
      else {
          printk(KERN_INFO "Yay, nope!\n");
          if(((struct file *)test)->private_data == fl->private_data)
              printk(KERN_INFO "But sadly same....\n");
      }
  }*/

  return 0;
}

int waker_open(struct inode *ine, struct file *fl) {
#ifdef DEBUG_USM
  printk(KERN_INFO "Opening waker..\n");
#endif
  if (!waker_state) {
    waker_state++;
    return 0;
  } else
    return 1;
}

int waker_release(struct inode *ine, struct file *fl) {
  printk(KERN_INFO "Releasing waker..\n");
  /* Temporary.. release through uffd's thingy. */
  /*while (usm_list) {
      void *temp = usm_list->next;
      int it = 0;
      while(it<100) {
          if(usm_list->uthread[it])
              __free_page(PFN_DOWN(virt_to_phys(usm_list->uthread[it])));
          it++;
      }
      kfree(usm_list);
      usm_list=temp;
  }*/
  waker_state = 0;
  mapped_sleepers = 0;
  init_waitqueue_head(&usm_wait);
  return 0;
}

int christine_release(struct inode *ine, struct file *fl) {
#ifdef DEBUG_USM
  printk(KERN_INFO "Releasing Christine.. ; %d virtually* down.\n",
         mapped_vCPUs); // (* : per instance it's not yet.)
#endif
  /* Temporary.. release through uffd's thingy. */
  /*while (usm_list) {
      void *temp = usm_list->next;
      int it = 0;
      while(it<100) {
          if(usm_list->uthread[it])
              __free_page(PFN_DOWN(virt_to_phys(usm_list->uthread[it])));
          it++;
      }
      kfree(usm_list);
      usm_list=temp;
  }*/
  mapped_vCPUs = 0;
  return 0;
}

static const struct proc_ops usm_fops = {
    // file_operations
    .proc_poll = usm_poll,
    .proc_read = usm_read,
    .proc_write = usm_write,
    .proc_open = usm_open //,
    //.proc_mmap 		= usm_mmap, .. here, the friggin' mmap.... for
    //them com. boiz!.... oh, and MAKE THE FIRST BIT ALWAYS BE STATE! SIMPLE..
    //FRIGGIN' SIMPLE! Okaay okay no problemo, we'll do dat. | okaay.. not the
    //first one : last one. At least for now. | AND not here, lul.
    //.proc_ioctl 	= usm_ioctl,
    //.proc_release 	= usm_release		// TODO imp.!
    //...proc_open mndty?
};

static const struct proc_ops waker_fops = { // file_operations
    .proc_open = waker_open,
    .proc_poll = usm_poll_sleepers,
    .proc_mmap = usm_mmap_sleepers, // but here! Hehe.
    .proc_release = waker_release};

static const struct proc_ops christine_fops = { // file_operations
    //.proc_open      = christine_open,       // man.. no friggin' multiple
    //Christine instances for now.. TODO
    //.proc_poll		= usm_poll_christine,
    .proc_mmap = usm_mmap_christine, // but here! Hehe.
    .proc_release = christine_release};

static struct file_operations cma_malloc_fileops = {
    .poll = usm_poll_sleepers, // testie..
    .mmap = usm_mmap,
    .release = usm_misc_f_release};

/*static struct file_operations mt_fileops = {
    .mmap           =   usm_mt_mmap,
    //.release        =   usm_misc_f_release
};*/

static struct miscdevice usm_miscdevice = {.minor = MISC_DYNAMIC_MINOR,
                                           .name = "USMMcd",
                                           .fops = &cma_malloc_fileops,
                                           .mode = 0777};

/*static struct miscdevice usm_mthread_miscdevice = {
    .minor           =   MISC_DYNAMIC_MINOR,
    .name            =   "USMMTcd",
    .fops            =   &mt_fileops,
    .mode            =   0777
};*/

// static DECLARE_WAIT_QUEUE_HEAD(usm_pagesWait); // move it there..
// static int verPs = 0;
__poll_t usm_poll_pages(struct file *file, poll_table *wait) {
  struct usm_ctx *usm_context = file->private_data;
  __poll_t ret = EPOLLIN;
  spin_lock(&usm_context->usmPagesLock);
  if (usm_context->usmPagesCount > 0) {
    // verPs++;
    // ret=POLLIN;
    return ret;
  }
  spin_unlock(&usm_context->usmPagesLock);
  poll_wait(file, &usm_context->usm_pagesWait, wait);
  return ret;
}

ssize_t usm_read_page(struct file *file, char __user *buf, size_t count,
                      loff_t *f_pos) {
  // struct usm_ctx *usm_context = file->private_data;
  unsigned long pfn = 0;
  struct page *page;
  // if (verPs) {
  if (count < sizeof(pfn)) {
    printk(KERN_INFO "Size nah gud (%ld) #USMReadHandleUsmRMPGs\n", count);
    goto outNope;
  }
  // verPs--;
  pfn = handle_page();
  page = pfn_to_page(pfn);
  if (page) {
    // memset(buf+(pfn-index)*4096, 0, 4096);
    // void * toZero=kmap(page);
    // memset(toZero, 0, 4096); /* Should / shall be done somewhere else */
    // kunmap(page);
    // printk("Passed\n");
    atomic_set(&(page)->_mapcount, -1);
    atomic_set(&(page)->_refcount, 1);
    set_bit(
        PG_usm,
        &page->flags); // it is lost somewhere.. TODO ..investigate it srsly..
    page->page_type = PG_usm;
    // pg->mapping=NULL;
    // pg->page_type = PG_usm;
    // set_bit(PG_uptodate, &page->flags);
    /*page->memcg_data=0;
    page->flags=0;
    page->private=0;*/
    // flush_dcache_page(page);
    // ClearPageHWPoisonTakenOff(page);

    /* if (pfn-index==0)
        printk("\tGud!\n"); */
    //__page_cache_release(page);
    /*
    ClearPageActive(page);
    ClearPageReferenced(page);
    */
    // ClearPageSwapBacked(page);
  } else { // ifdef TODO
    printk("[mayday/usm_read_page] No page linked to fetched PFN : %lu.\n",
           pfn);
    return -EFAULT;
  }
  if (copy_to_user((__u64 __user *)buf, &pfn, sizeof(pfn))) {
    printk(KERN_INFO "[mayday/usm_read_page] Failed copy to user!\n");
    goto outNope;
  }

  return count;
  // }
outNope:
  return -EAGAIN;
}

ssize_t usm_instance(struct file *file, const char __user *buf, size_t count,
                     loff_t *f_pos) {
  struct usm_ctx *sailor = usm_list;
  if (!sailor) {
#ifdef DEBUG_USM
    printk(KERN_INFO "[USM] No USM instance up!\n");
#endif
    return 1;
  }
#ifdef DEBUG_USM
  printk(KERN_INFO "[USM] Linking to %lu instance!\n", count);
#endif
  while (count && sailor && sailor->next) {
    sailor = sailor->next;
    count--;
  }
  if (count) {
    printk(KERN_INFO "[USM] Not enough up!\t%lu to go!\n", count);
    if (!sailor)
      printk(KERN_INFO "[USM] Not even one up..\n");
    return 1;
  }
  current->usm_x = (void *)sailor;
  if (buf) {
#ifdef DEBUG_USM
    printk(KERN_INFO "[USM] Non NULL buf., assuming UTs!\n");
#endif
    current->mm->def_flags |= VM_USM_UT;
  }
  return 0;
}

static const struct proc_ops usm_pages_fops = {.proc_poll = usm_poll_pages,
                                               .proc_read = usm_read_page,
                                               .proc_write = usm_instance,
                                               .proc_open = usm_open};

static int __init hwusm_lkm_init(void) {
  int ret = misc_register(&usm_miscdevice);
  struct page *pg;
  if (unlikely(ret)) {
    printk("Misc. dev. reg. failed : %d.\n", ret);
    return -EAGAIN;
  } else {
    dev = *usm_miscdevice.this_device;
    dev.dma_mask = ((u64 *)DMA_BIT_MASK(64));
    dev.coherent_dma_mask = (u64)DMA_BIT_MASK(64);
  }
  /*ret = misc_register(&usm_mthread_miscdevice);
  if (unlikely(ret)) {
      printk("Sec. misc. dev. reg. failed : %d.\n", ret);
      return -EAGAIN;
  }*/
  proc_entry = proc_create("usm", 0777, NULL, &usm_fops);
  if (!proc_entry) {
    printk(KERN_INFO "Processes' USM tagging file operations not created!\n");
    misc_deregister(&usm_miscdevice);
    return -EAGAIN;
  }
  proc_entry_sleepers = proc_create("usmSleepers", 0777, NULL, &waker_fops);
  if (!proc_entry_sleepers) {
    printk(KERN_INFO "Processes' USM waker file operations not created!\n");
    proc_remove(proc_entry);
    misc_deregister(&usm_miscdevice);
    return -EAGAIN;
  }
  proc_entry_christine_sleepers =
      proc_create("usmChristineSleepers", 0777, NULL,
                  &christine_fops); // each thread (k. or vCPU in Christine's
                                    // eyes) will have to map ONE ID zone.
  if (!proc_entry_christine_sleepers) {
    printk(KERN_INFO
           "Processes' USMChristine waker file operations not created!\n");
    proc_remove(proc_entry);
    proc_remove(proc_entry_sleepers);
    misc_deregister(&usm_miscdevice);
    return -EAGAIN;
  }
  proc_entry_pgs = proc_create("usmPgs", 0777, NULL, &usm_pages_fops);
  if (!proc_entry_pgs) {
    printk(KERN_INFO "USM pages' freeing file operations not created!\n");
    proc_remove(proc_entry);
    proc_remove(proc_entry_sleepers);
    proc_remove(proc_entry_christine_sleepers);
    misc_deregister(&usm_miscdevice);
    return -EAGAIN;
  }
  buf = dma_alloc_coherent(&dev, usmMemSize, &paddr, GFP_KERNEL);
  if (!buf) {
    printk(KERN_WARNING "[Mayday] Unable to allocate USM's CM pool!\n");
    proc_remove(proc_entry);
    proc_remove(proc_entry_sleepers);
    proc_remove(proc_entry_christine_sleepers);
    proc_remove(proc_entry_pgs);
    misc_deregister(&usm_miscdevice);
    return -EAGAIN;
  }
  waker_state = 0;
  mapped_sleepers = 0;
  mapped_vCPUs = 0;
  init_waitqueue_head(&usm_wait);
  INIT_LIST_HEAD(&usm_sleepers_map_list);
  usm_list = NULL;
  unmap_sleepers = 0;
  pgoff = 0;
  printk(KERN_INFO "Yo! #USMRM\n");
  pg = pfn_to_page(PFN_DOWN(virt_to_phys(phys_to_virt(paddr))));
  printk(KERN_INFO
         "Base state : %d - %d - %d - %d - %d - %d - %d - %d - %d - %d\n",
         pg->flags & PG_slab, pg->flags & PG_reserved, pg->flags & PG_lru,
         pg->flags & PG_private, pg->flags & PG_private_2,
         pg->flags & PG_active, pg->page_type == PG_slab,
         pg->memcg_data == (unsigned long)NULL, atomic_read(&(pg->_refcount)),
         atomic_read(&(pg->_mapcount)));
  return 0;
}

static void __exit hwusm_lkm_exit(void) {
  struct page *pg;
  unsigned long pfn = PFN_DOWN(virt_to_phys(phys_to_virt(paddr)));
  int found = 0, nfound = 0, nnfound = 0;
  int info = 0, infox = 0, infoxp = 0;
  pg = pfn_to_page(pfn);
  info = atomic_read(&(pg->_refcount));
  infox = atomic_read(&(pg->_mapcount));
  indext = usmMemSize;
  printk(KERN_INFO "Ciaossu! #USMRM\n");
  for (int i = 0; i < indext / PAGE_SIZE; i++) {
    pg = pfn_to_page(pfn + i);
    if (pg) {
      if (!test_bit(PG_usm, &pg->flags)) {
        nfound++;
        // printk(KERN_INFO "[USM/Module/Exit] Unrecognized page!");
        // continue;
      }
      found++;
      clear_bit(PG_usm, &pg->flags);
      clear_bit(PG_active, &pg->flags);
      clear_bit(PG_private, &pg->flags);
      clear_bit(PG_private_2, &pg->flags);
      clear_bit(PG_writeback, &pg->flags);
      clear_bit(PG_reserved, &pg->flags);
      clear_bit(PG_slab, &pg->flags);
      clear_bit(PG_lru, &pg->flags);
      pg->page_type = PG_reclaim; // PG_slab.. yup, nope. (PG_FLGS_CHCK_A_F)
                                  // // TODO : investigate this
      pg->mapping = NULL;
      pg->memcg_data = (unsigned long)NULL;
      atomic_set(&(pg->_mapcount), -1);
      infoxp = atomic_read(&(pg->_mapcount));
      atomic_set(&(pg->_refcount), 1);
    } else {
      printk(KERN_WARNING "[USM/Module/Exit] Unexisting page with PFN %d", i);
      nnfound++;
    }
  }
  dma_free_coherent(&dev /*usm_miscdevice.this_device*/, usmMemSize, buf,
                    paddr);
  if (found)
    printk(KERN_INFO "found!\t%d", found);
  if (nfound)
    printk(KERN_INFO "nfound!\t%d", nfound);
  if (nnfound)
    printk(KERN_INFO "nnfound!\t%d", nnfound);
  printk(KERN_INFO "info infox infoxp %d %d %d\n", info, infox, infoxp);
  pg = pfn_to_page(pfn);
  printk(KERN_INFO "%d\t%d\n", atomic_read(&(pg->_refcount)),
         atomic_read(&(pg->_mapcount)));
  proc_remove(proc_entry);
  proc_remove(proc_entry_sleepers);
  proc_remove(proc_entry_christine_sleepers);
  proc_remove(proc_entry_pgs);
  while (usm_list) {
    void *temp = usm_list->next;
    int it = 0;
    while (it < MAX_UTHREADS) {
      if (usm_list->uthread[it])
        __free_page(pfn_to_page(PFN_DOWN(virt_to_phys(usm_list->uthread[it]))));
      it++;
    }
    kfree(usm_list);
    usm_list = temp;
  }
  misc_deregister(&usm_miscdevice);
}

module_init(hwusm_lkm_init);
module_exit(hwusm_lkm_exit);
