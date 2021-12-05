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
    if (f->page == page && f->file_node == node) {
      return f;
    }
  }
  return NULL;
}

struct frame *
find_free_frame () {
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    f = list_entry (e, struct frame, elem);
    if (!f->page) {
      list_remove (e);
      list_push_back (&table.frames, e);
      return f;
    } else if (f->accessed)
      f->accessed = false;  
  }
  return NULL;
}

struct frame *
alloc_frame (void *page)
{
  lock_acquire (&frame_lock);
  struct frame *f;

  if (!writable && node) {
    f = locate_frame (page, node);
    if (f && !f->writable) {
      f->num_refs++;
      lock_release (&frame_lock);
      *shared = true;
      return f;
    }
  }

  f = find_free_frame();
  
  if (f) {
    f->page = page;
    f->writable = writable;
    f->num_refs++;
    f->file_node = node;
    allocated = true;
    lock_release (&frame_lock);
    return f;
  }
  PANIC ("alloc_frame: no free frames!"); 
}

void
free_frame (void *kpage)
{
  lock_acquire (&frame_lock);
  struct frame *f = NULL;
  for (struct list_elem *e = list_begin (&table.frames);
       e != list_end (&table.frames);
       e = list_next (e))
  {
    struct frame *temp = list_entry (e, struct frame, elem);
    if (temp->kPage == kpage) {
      f = temp;
      break;
    }
  }
  if (!f || f->num_refs--) {
    lock_release (&frame_lock);
    return;
  }

  f->page = NULL;
  list_remove (&f->elem);
  list_push_front (&table.frames, &f->elem);
  
  lock_release (&frame_lock);
}

void size () {
  printf("Size is %d\n", list_size(&table.frames));
}
