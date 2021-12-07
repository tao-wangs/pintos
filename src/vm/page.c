#include "vm/page.h"
#include <hash.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
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

/* Initialises supplemental page table for the thread. */
struct page_table * 
pagetable_init (void)
{
  struct page_table *page_table = malloc(sizeof(struct page_table));
  hash_init (&page_table->table, page_hash, page_less, NULL);
  lock_init (&page_table->lock);
  return page_table;
}

/* Locates and returns the supplemental page conntaining the page addr. */
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

/* Creates a supplemental page with addr and inserts it to thread's supplemental page table. */
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

/* Removes the supplemental page with addr from thread's supplemental page table. */
void
remove_page (void *addr, struct page_table *page_table)
{
  struct page *page = locate_page (addr, page_table); 
  if (!page)
    return;
  lock_acquire (&page_table->lock);
  hash_delete (&page_table->table, &page->elem);
  lock_release (&page_table->lock);
  free (page);
}

/* Destroys the thread's supplemental page table, freeing its resources. */
void 
pagetable_destroy (struct page_table *page_table)
{
  hash_destroy (&page_table->table, page_remove);
}

/* Auxillary for pagetable_destroy. */
static void
page_remove (struct hash_elem *e, void *aux UNUSED)
{
  free (hash_entry (e, struct page, elem));
}