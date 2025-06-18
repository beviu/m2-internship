#ifndef ALLOC_H
#define ALLOC_H
#include "../../com/com.h"
/*
        - Toute politique dans USM doit implémenter cette interface.
        - Le runtime d'USM appellera ces fonctions
        - Pour l'instant l'on ne connait pas encore tous les types de retours et
   les paramètres.
        - La liste évidemment n'est pas exhaustive
*/

#define FAULT_EXISTS 7
#define FAULT_OOM_VICTIM 5

extern struct usm_alloc_policy_ops {
  char *usm_alloc_policy_name;
  int (*usm_init)(); // initialise la politique. Par exemple les structures de
                     // données de la politique peuvent être initialisées dans
                     // cette fonction.
  int (*usm_alloc)(struct usm_event *event); // alloue de la mémoire physique
  int (*usm_pindex_free)(struct usm_event *event); // libère la mémoire
                                                   // phyisique
  int (*usm_free)(struct usm_event *event); // libère la mémoire physique
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
} default_alloc_policy;

struct usm_policy { // struct usm_alloc_policy
  char *name;
  unsigned long ops; // struct usm_alloc_policy_ops
};

extern int (*get_pages)(struct list_head *placeholder, int nr);
extern void (*put_pages)(struct list_head *pages);
extern void (*restore_used_page)(struct list_head *page);
extern void (*hold_used_page)(struct list_head *page);
#endif