#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <list.h>
#include "page.h"
#include "devices/block.h"

struct swapslot{
  struct list_elem elem;
  block_sector_t sector;
};

struct swap {
  struct list slots;
};

void swaptable_init(void);
void swaptable_free(void);
void free_swapslot(struct swapslot *slot);
void evict_to_swap(void *addr);
void get_from_swap(struct page *page, void* kpage);

#endif /* vm/swap.h */
