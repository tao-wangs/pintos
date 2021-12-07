#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include <hash.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include <stdio.h>

static void page_remove (struct hash_elem *e, void *aux UNUSED);

static unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  struct page* elem = hash_entry (e, struct page, elem);
  return hash_int ((int) elem->addr);
}

static bool
page_less (const struct hash_elem *a,
           const struct hash_elem *b,
           void *aux UNUSED)
{
  return hash_entry (a, struct page, elem)->addr
       < hash_entry (b, struct page, elem)->addr;
}

struct page_table* 
pagetable_init (void)
{
  struct page_table *page_table = malloc(sizeof(struct page_table));
  hash_init (&page_table->table, page_hash, page_less, NULL);
  lock_init (&page_table->lock);
  return page_table;
}

struct page *
locate_page (void *addr, struct page_table *page_table)
{
  void *page = pg_round_down (addr); 
  struct page temp; 
  temp.addr = page;
  lock_acquire (&page_table->lock);
  struct hash_elem *e = hash_find (&page_table->table, &temp.elem);
  lock_release (&page_table->lock);
  if (!e)
  {
    return NULL;  
  }
  struct page *p = hash_entry (e, struct page, elem);
  return p;
}

void
add_page (void *addr, void *data, enum page_status status, struct page_table *page_table, bool writable)
{
  void *pg_addr = pg_round_down (addr);
  struct page *page = malloc (sizeof (struct page));
  if (!page)
  {
    PANIC ("failed to allocate supplemental page table entry");
  }
  page->addr = pg_addr;
  page->data = data;
  page->status = status;
  page->writable = writable;
  page->t = thread_current ();
  lock_acquire (&page_table->lock);
  hash_insert (&page_table->table, &page->elem);
  lock_release (&page_table->lock);
}

void
remove_page (void *addr, struct page_table *page_table)
{
  struct page *page = locate_page (addr, page_table); 
  if (!page)
    return;
  pagedir_clear_page (page->t->pagedir, addr);
  lock_acquire (&page_table->lock);
  switch (page->status)
  { 
    case FRAME:
      free_frame (addr);
      break;
    case FILE_SYS:
      free (page->data);
      break;
    case SWAP:  
      free_swapslot ((struct swapslot *) page->data, page);
    case ZERO:
      break;
  }
  hash_delete (&page_table->table, &page->elem);
  lock_release (&page_table->lock);
  free (page);
}
static void
page_remove (struct hash_elem *e, void *aux UNUSED)
{
  struct page *p = hash_entry (e, struct page, elem);
  remove_page (p->addr, p->t->page_table);
}

void pagetable_destroy (struct page_table *page_table)
{
  hash_destroy (&page_table->table, page_remove);
}
