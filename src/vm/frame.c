#include "vm/frame.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include <stdio.h>
#include "vm/swap.h"
#include "userprog/pagedir.h"

struct frametable table;
struct lock frame_lock;

extern uint32_t init_ram_pages;

void 
frametable_init (void)
{ 
  int j = 0;
  list_init (&table.frames);
  lock_init (&frame_lock);
  for (int i = 0; i < 367; ++i)
  {
    struct frame *f = malloc (sizeof (struct frame));
    if (!f)
      PANIC ("Failed to malloc frame");
    f->kPage = palloc_get_page (PAL_USER);
    if (!f->kPage)
      PANIC ("FAILED to palloc frame");
    f->num_refs = 0;
    f->page = NULL;
    f->accessed = false;
    f->fid = j;
    list_init (&f->page_list);
    lock_init (&f->lock);
    list_push_back (&table.frames, &f->elem);
    j++;
  }

}

void
frametable_free (void)
{
  struct list_elem *e = list_begin (&table.frames);
  while (e != list_end (&table.frames))
  {
    struct list_elem *next = list_next (e);
    free (list_entry (e, struct frame, elem));
    e = next;
  } 
}

// Protected by frame lock before calling
struct frame *
locate_frame (void *page, struct inode *node) 
{
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    lock_acquire (&f->lock);
    if (f->page == page && f->file_node == node) {
      return f;
    }
    lock_release (&f->lock);
  }
  return NULL;
}

struct frame *
find_free_frame () 
{
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    lock_acquire (&f->lock);
    if (!f->page)
      return f;
    else if (f->accessed)
      f->accessed = false;  
    else
    {
      struct swapslot * slot = evict_to_swap (f->page, f->kPage);
      f->page = NULL;
      slot->num_refs = f->num_refs;
      f->num_refs = 0;
      struct list_elem *e = list_begin (&f->page_list);
      while (e != list_end (&f->page_list))
      {
	    struct list_elem *next = list_next (e);
        list_remove (e);
	    struct page *page = list_entry (e, struct page, page_elem);
	    list_push_back (&slot->page_list, &page->swap_elem);
        page->status = SWAP;
        page->data = slot;
	    pagedir_clear_page (page->t->pagedir, page->addr);
        e = next;
      }
      return f;
    }
    lock_release (&f->lock);
  }
  return NULL;
}

struct frame *
alloc_frame (struct page *page, bool writable, struct inode *node, bool *shared)
{
  struct frame *f;

  if (!writable && node) {
    f = locate_frame (page->addr, node);
    if (f && !f->writable) {
      f->num_refs++;
      list_push_back (&f->page_list, &page->page_elem);
      lock_release (&f->lock);
      *shared = true;
      return f;
    }
    lock_release (&f->lock);
  }

  f = find_free_frame();
  
  if (f) {
    list_remove (&f->elem);
    list_push_back (&table.frames, &f->elem);
    list_push_back (&f->page_list, &page->page_elem);
    f->page = page->addr;
    f->writable = writable;
    f->num_refs++;
    f->file_node = node;
    lock_release (&f->lock);
    return f;
  }
  PANIC ("alloc_frame: no free frames!"); 
}

void
free_frame (void *kpage)
{
  struct frame *f = NULL;
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    struct frame *temp = list_entry (e, struct frame, elem);
    lock_acquire (&temp->lock);
    if (temp->kPage == kpage) {
      f = temp;
      break;
    }
    lock_release (&temp->lock);
  }

  if (!f) {
    return;
  }

  struct list_elem *e = list_begin (&f->page_list);
  while (e != list_end (&f->page_list))
  {
    struct list_elem *next = list_next (e);
    struct page *page = list_entry(e, struct page, page_elem);
    if (page->t == thread_current ())
    {
      list_remove (&page->page_elem);
      break;
    }
    e = next;
  }

  if (--f->num_refs){
    lock_release (&f->lock);
    return;
  }

  f->page = NULL;
  f->file_node = NULL;
  list_remove (&f->elem);
  list_push_front (&table.frames, &f->elem);
  
  lock_release (&f->lock);
}

void size () {
  printf("Size is %d\n", list_size(&table.frames));
}
