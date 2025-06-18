#include "../../../include/usm/usm.h"

/*
    Essentially helpers for the swap module
*/

struct usm_alloc_policy_ops default_alloc_policy;

int (*get_pages)(struct list_head *placeholder, int nr);
void (*put_pages)(struct list_head *pages);
void (*restore_used_page)(struct list_head *page);
void (*hold_used_page)(struct list_head *page);