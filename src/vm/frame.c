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

struct frame *
alloc_frame (void *page)
{
  bool allocated = false;
  lock_acquire (&frame_lock);
  struct frame *f;
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    f = list_entry (e, struct frame, elem);
    if (!f->page)
    {
      f->page = page;
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
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if (f->page == page)
    {
      f->page = NULL;
      list_remove (e);
      list_push_front (&table.frames, e);
    }
  }
  lock_release (&frame_lock);
}
