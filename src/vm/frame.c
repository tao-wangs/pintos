#include "vm/frame.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include <stdio.h>
#include "vm/swap.h"
#include "userprog/pagedir.h"
#include "vm/locklist.h"

struct frametable table;    /* Frame table. */
struct lock frame_lock;     /* Frame lock. */

extern uint32_t init_ram_pages;

/* Initialises frame table, frame lock and frames in user pool. */
void 
frametable_init (void)
{ 
  locklist_init (&table.frames);
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
    locklist_init (&f->page_list);
    elem_init (&f->elem);
    locklist_push_back (&table.frames, &f->elem);
  }

}

/* Frees frame table. */
void
frametable_free (void)
{
  struct locklist_elem *e = locklist_begin (&table.frames);
  while (e != locklist_end (&table.frames))
  {
    struct locklist_elem *next = locklist_remove (e);
    lock_release (&e->lock);
    free (list_entry (e, struct frame, elem));
    e = next;
  } 
}

/* Locates frame with contains the page.  
   Protected by frame lock before calling. */
struct frame *
locate_frame (void *page, struct inode *node) 
{
  for (struct locklist_elem *e = locklist_begin (&table.frames);
       e != locklist_end (&table.frames);
       e = locklist_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if (f->page == page && f->file_node == node) {
      return f;
    }
  }
  lock_release (&table.frames.tail.prev->lock); 
  lock_release (&table.frames.tail.lock);
  return NULL;
}

/* Allocates a page to a frame. */
struct frame *
find_free_frame () 
{
  for (struct locklist_elem *e = locklist_begin (&table.frames);
       e != locklist_end (&table.frames);
       e = locklist_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if (!f->accessed)
    {
      for (struct locklist_elem *ep = locklist_begin (&f->page_list);
           ep != locklist_end (&f->page_list);
           ep = locklist_next (ep))
      {
        struct page *page = list_entry (ep, struct page, page_elem);
        if (pagedir_is_accessed (page->t->pagedir, page->addr))
        {
          f->accessed = true;
          pagedir_set_accessed (page->t->pagedir, page->addr, false);
        }
      }
      lock_release (&f->page_list.tail.prev->lock);    
      lock_release (&f->page_list.tail.lock);    
    }
    if (!f->page)
      return f;
    else if (f->accessed)
      f->accessed = false;  
    else
    {
      evict_to_swap (f);
      return f;
    }
  }
  lock_release (&table.frames.tail.prev->lock);
  lock_release (&table.frames.tail.lock);
  return NULL;
}

struct frame *
alloc_frame (struct page *page, bool writable, struct inode *node, bool *shared)
{
  struct frame *f;

  if (!writable && node) {
    f = locate_frame (page->addr, node);
    if (f && !f->writable) 
    {
      f->num_refs++;
      lock_acquire (&page->page_elem.lock);
      locklist_push_back (&f->page_list, &page->page_elem);
      lock_release (&page->page_elem.lock);
      *shared = true;
      lock_release (&f->elem.prev->lock);
      lock_release (&f->elem.lock);
      lock_release (&f->elem.next->lock);
      return f;
    }
    if (f)
    {
      lock_release (&f->elem.prev->lock);
      lock_release (&f->elem.lock);
      lock_release (&f->elem.next->lock);
    }
  }

  f = find_free_frame ();
  if (!f)
    f = find_free_frame ();
  
  if (f) {
    locklist_remove (&f->elem);
    lock_release (&f->elem.prev->lock);
    lock_release (&f->elem.next->lock);
    if (f->elem.next->next != NULL)
      lock_release (&f->elem.next->next->lock);
    locklist_push_back (&table.frames, &f->elem);
    lock_acquire (&page->page_elem.lock);
    locklist_push_back (&f->page_list, &page->page_elem);
    lock_release (&page->page_elem.lock);
    f->page = page->addr;
    f->writable = writable;
    f->num_refs++;
    f->file_node = node;
    lock_release (&f->elem.lock);
    return f;
  }
  PANIC ("alloc_frame: no free frames!"); 
}

/* Frees the frame in the user pool with kernel address kpage */
void
free_frame (void *kpage)
{
  struct frame *f = NULL;
  for (struct locklist_elem *e = locklist_begin (&table.frames);
       e != locklist_end (&table.frames);
       e = locklist_next (e))
  {
    struct frame *temp = list_entry (e, struct frame, elem);
    if (temp->kPage == kpage) {
      f = temp;
      break;
    }
  }

  if (!f) {
    lock_release (&table.frames.tail.prev->lock);
    lock_release (&table.frames.tail.lock);
    return;
  }

  struct locklist_elem *e = locklist_begin (&f->page_list);
  while (e != locklist_end (&f->page_list))
  {
    struct page *page = list_entry(e, struct page, page_elem);
    if (page->t == thread_current ())
    {
      locklist_remove (&page->page_elem);
      lock_release (&e->prev->lock);
      lock_release (&e->next->lock);
      if (e->next->next != NULL)
        lock_release (&e->next->next->lock);
      break;
    } else
    {
      e = locklist_next (e);
    }
  }
  lock_release (&e->lock);
  ASSERT (e != locklist_end (&f->page_list));

  if (--f->num_refs){
    lock_release (&f->elem.prev->lock);
    lock_release (&f->elem.lock);
    lock_release (&f->elem.next->lock);
    return;
  }

  f->page = NULL;
  f->file_node = NULL;
  locklist_remove (&f->elem);
  lock_release (&f->elem.prev->lock);
  lock_release (&f->elem.next->lock);
  if (f->elem.next->next != NULL)
    lock_release (&f->elem.next->next->lock);
  locklist_push_front (&table.frames, &f->elem);
  lock_release (&f->elem.lock); 
}

void size () {
  printf("Size is %d\n", locklist_size(&table.frames));
}
