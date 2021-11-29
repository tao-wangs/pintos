#include "vm/frame.h"
#include "threads/loader.h"
#include "threads/malloc.h"

struct frametable table;

extern uint32_t init_ram_pages;

void 
frametable_init (void)
{
  list_init (&table.frames);
  for (int i = 0; i < init_ram_pages; ++i)
  {
    struct frame *f = malloc (sizeof (struct frame));
    if (!f)
    {
      PANIC ("Failed to malloc frame");
    }
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

void
alloc_frame (void *page)
{
  bool allocated = false;
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if (!f->page)
    {
      f->page = page;
      allocated = true;
      break;
    }
  }

  if (!allocated)
  {
    PANIC ("alloc_frame: no free frames!"); 
  }
}

void
free_frame (void *page)
{
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
}
