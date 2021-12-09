#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <list.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "devices/block.h"

struct swapslot{
  struct list_elem elem;
  block_sector_t sector;
  struct list page_list;
  int num_refs;
  struct semaphore sema;
};

struct swap {
  struct list slots;
};

void swaptable_init(void);
void swaptable_free(void);
void free_swapslot(struct swapslot *slot, struct page *page);
void evict_to_swap(struct frame *frame);
void get_from_swap(struct page *page, struct frame *frame);

#endif /* vm/swap.h */
