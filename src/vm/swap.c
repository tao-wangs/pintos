#include <inttypes.h>
#include "swap.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"

struct swap table;
struct lock swap_lock;
struct block *swap;

static struct swapslot *pop_free_slot(void);

void swaptable_init(void){
  list_init(&table.slots);
  lock_init(&swap_lock);
  swap = block_get_role(BLOCK_SWAP);
  for (uint32_t i = 0; i < block_size(swap); i+=PGSIZE/BLOCK_SECTOR_SIZE){
    struct swapslot *slot = malloc (sizeof(struct swapslot));
    if (!slot){
    	PANIC("Failed to allocate swapslot\n");
    }
    slot->sector = i; //might have to be offset by 1
    list_push_back(&table.slots, &slot->elem);
  }
}

void swaptable_free(void){
  struct list_elem *e = list_begin(&table.slots);
  while (e != list_end(&table.slots)){
    struct list_elem *next = list_next(e);
    free(list_entry(e, struct swapslot, elem));
    e = next; 
  }
}

void free_swapslot(struct swapslot *slot){
  free(slot);
}

static struct swapslot
*pop_free_slot(void){
  if (!list_empty(&table.slots)) {
    struct list_elem *e = list_pop_front(&table.slots);
    return list_entry(e, struct swapslot, elem);
  }
  return NULL;
}

void evict_to_swap(struct page *page){
  lock_acquire(&swap_lock);
  struct swapslot *free_slot = pop_free_slot();
  if (free_slot == NULL){
    PANIC("Swap is full!!!");
  } 
  void* kpage = pagedir_get_page(thread_current()->pagedir, page->addr);
  for (int i = 0; i < PGSIZE/BLOCK_SECTOR_SIZE; i++){
    block_write(swap, free_slot->sector+i, kpage+(BLOCK_SECTOR_SIZE*i));
  }
  page->data = free_slot;
  page->status = SWAP;
  lock_release(&swap_lock);
}

void get_from_swap(struct page *page, void *kpage){
  lock_acquire(&swap_lock);
  ASSERT(page->status == SWAP);
  ASSERT(kpage != NULL);
  struct swapslot *free_slot = (struct swapslot *) page->data;
  list_push_back(&table.slots, &free_slot->elem);
  for (int i = 0; i< PGSIZE/BLOCK_SECTOR_SIZE; i++){
    block_read(swap, free_slot->sector+i, kpage+(BLOCK_SECTOR_SIZE*i));
  }
  page->data = NULL;
  page->status = FRAME;
  lock_release(&swap_lock);
}

