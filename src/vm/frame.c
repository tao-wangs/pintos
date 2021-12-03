#include "vm/frame.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include <stdio.h>

struct frametable table;
struct lock frame_lock;

extern uint32_t init_ram_pages;

void 
frametable_init (void)
{
  list_init (&table.frames);
  lock_init (&frame_lock);
  printf ("init_ram_pages: %d\n", init_ram_pages);
  for (int i = 0; i < 367; ++i)
  {
    //printf ("Initialising frame %d\n", i);
    struct frame *f = malloc (sizeof (struct frame));
    if (!f)
      PANIC ("Failed to malloc frame");
    f->kPage = palloc_get_page (PAL_USER);
    f->num_refs = 0;
    if (!f->kPage)
      PANIC ("FAILED to palloc frame");
    list_push_back (&table.frames, &f->elem);
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
locate_frame (void *page) 
{
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if (f->page == page) {
      return f;
    }
  }
  return NULL;
}

struct frame *
alloc_frame (void *page, bool writable)
{
  
  bool allocated = false;
  lock_acquire (&frame_lock);

  struct frame *f;

  if (!writable) {
    f = locate_frame (page);
    if (f && !f->writable) {
      f->num_refs++;
      return f;
    }
  }
  
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    f = list_entry (e, struct frame, elem);
    if (!f->page)
    {
      f->page = page;
      f->writable = writable;
      f->num_refs++;
      allocated = true;
      break;
    }
  }
  
  lock_release (&frame_lock);
  if (!allocated)
  {
    PANIC ("alloc_frame: no free frames!"); 
  }
  return f;
}

void
free_frame (void *page)
{
  lock_acquire (&frame_lock);
  struct frame *f = locate_frame (page);

  if (!f || f->num_refs--) {
    lock_release (&frame_lock);
    return;
  }

  f->page = NULL;
  list_remove (&f->elem);
  list_push_front (&table.frames, &f->elem);
  
  lock_release (&frame_lock);
}
