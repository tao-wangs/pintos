#include <inttypes.h>
#include "swap.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "vm/frame.h"

struct swap table2;
struct lock swap_lock;
struct block *swap;

static struct swapslot *pop_free_slot(void);

void swaptable_init(void){
  //printf("INside swap table sdakljksladlskjklsjkldjklsjkldsajlksadjldsakjsdklsjdalasdjlksadjklsdajlksaDSLJDSKLJDSLKJSKDLJDSKLJSLKJDLSd\n");
  //size();
  list_init(&table2.slots);
  //size(); 
  lock_init(&swap_lock);

  swap = block_get_role(BLOCK_SWAP);
  for (uint32_t i = 0; i < block_size(swap); i+=PGSIZE/BLOCK_SECTOR_SIZE){
    struct swapslot *slot = malloc (sizeof(struct swapslot));
    if (!slot){
    	PANIC("Failed to allocate swapslot\n");
    }
    slot->sector = i; //might have to be offset by 1
    list_init (&slot->page_list);
    list_push_back(&table2.slots, &slot->elem);
  }
}

void swaptable_free(void){
  struct list_elem *e = list_begin(&table2.slots);
  while (e != list_end(&table2.slots)){
    struct list_elem *next = list_next(e);
    free(list_entry(e, struct swapslot, elem));
    e = next; 
  }
}

void free_swapslot(struct swapslot *slot, struct page *page){
  lock_acquire (&swap_lock);
  list_remove (&page->swap_elem); 
  if (--slot->num_refs)
  {
    lock_release (&swap_lock);
    return;
  }
  list_push_back (&table2.slots, &slot->elem);
  lock_release (&swap_lock);
}

static struct swapslot *
pop_free_slot(void)
{
  if (!list_empty (&table2.slots)) 
  {
    struct list_elem *e = list_pop_front (&table2.slots);
    return list_entry (e, struct swapslot, elem);
  }
  return NULL;
}

void
evict_to_swap(struct frame *frame)
{
  lock_acquire (&swap_lock);
  struct swapslot *free_slot = pop_free_slot ();
  if (free_slot == NULL){
    PANIC ("Swap is full!!!");
  } 
  for (int i = 0; i < PGSIZE/BLOCK_SECTOR_SIZE; i++){
    block_write (swap, free_slot->sector + i, frame->kPage + (BLOCK_SECTOR_SIZE * i));
  }
  frame->page = NULL;
  free_slot->num_refs = frame->num_refs;
  frame->num_refs = 0;
  struct locklist_elem *e = locklist_begin (&frame->page_list);
  while (e != locklist_end (&frame->page_list))
  {
    struct locklist_elem *next = locklist_remove (e);
    struct page *page = list_entry (e, struct page, page_elem);
    list_push_back (&free_slot->page_list, &page->swap_elem);
    page->status = SWAP;
    page->data = free_slot;
    pagedir_clear_page (page->t->pagedir, page->addr);
    lock_release (&e->lock);
    e = next;
  }
  lock_release (&frame->page_list.head.lock);
  lock_release (&frame->page_list.tail.lock);
  lock_release(&swap_lock);
}

void get_from_swap(struct page *page, struct frame *frame){
  //PANIC ("SWAPTABLE FREE5\n");
  lock_acquire(&swap_lock);
  ASSERT(page->status == SWAP);
  ASSERT(frame != NULL);
  //printf ("getting page %p into frame %d\n", page->addr, frame->fid);
  struct swapslot *free_slot = (struct swapslot *) page->data;
  list_push_back(&table2.slots, &free_slot->elem);
  for (int i = 0; i< PGSIZE/BLOCK_SECTOR_SIZE; i++){
    block_read(swap, free_slot->sector+i, frame->kPage+(BLOCK_SECTOR_SIZE*i));
  }
  struct list_elem *e = list_begin (&free_slot->page_list);
  while (e != list_end (&free_slot->page_list))
  {
    struct list_elem *next = list_next (e);
    list_remove (e);
    struct page *new_page = list_entry (e, struct page, swap_elem);
    new_page->status = FRAME;
    new_page->data = NULL;
    if (new_page == page)
    {
      e = next;
      continue;
    }
    pagedir_set_page (new_page->t->pagedir, new_page->addr, frame->kPage, new_page->writable);
    lock_acquire (&new_page->page_elem.lock); 
    locklist_push_back (&frame->page_list, &new_page->page_elem);
    lock_release (&new_page->page_elem.lock); 
    e = next;
  }
  frame->num_refs = free_slot->num_refs;
  lock_release(&swap_lock);
}

