#include "vm/page.h"
#include <hash.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <stdio.h>

struct hash page_table;

struct lock page_lock;

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

void 
pagetable_init (void)
{
  hash_init (&page_table, page_hash, page_less, NULL);
  lock_init (&page_lock);
}

struct page *
locate_page (void *addr)
{
  void *page = pg_round_down (addr); 
  printf ("finding page at addr %p\n", page);
  struct page temp; 
  temp.addr = page;
  lock_acquire (&page_lock);
  struct hash_elem *e = hash_find (&page_table, &temp.elem);
  lock_release (&page_lock);
  if (!e)
  {
    return NULL;  
  }
  return hash_entry (e, struct page, elem);
}

void
add_page (void *addr, void *data, enum page_status status)
{
  void *pg_addr = pg_round_down (addr);
  printf ("adding page at addr %p\n", pg_addr);
  struct page *page = malloc (sizeof (struct page));
  if (!page)
  {
    PANIC ("failed to allocate supplemental page table entry");
  }
  page->addr = pg_addr;
  page->data = data;
  page->status = status;
  page->t = thread_current ();
  lock_acquire (&page_lock);
  hash_insert (&page_table, &page->elem);
  lock_release (&page_lock);
}

void
remove_page (void *addr)
{
  struct page *page = locate_page (addr); 
  if (!page)
    return;
  lock_acquire (&page_lock);
  hash_delete (&page_table, &page->elem);
  lock_release (&page_lock);
  free (page);
}
static void
page_remove (struct hash_elem *e, void *aux UNUSED)
{
  free (hash_entry (e, struct page, elem));
}

void pagetable_destroy (void)
{
  hash_destroy (&page_table, page_remove);
}
