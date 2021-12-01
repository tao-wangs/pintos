#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <hash.h>

enum page_status
{
   FRAME, 
   SWAP, 
   FILE_SYS,
   ZERO
};

struct page
{
  struct hash_elem elem;
  void *addr;
  enum page_status status;
  void *data;
};

static unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED);

static bool
page_less (const struct hash_elem *a,
           const struct hash_elem *b,
           void *aux UNUSED);

void pagetable_init (void);

struct page *locate_page (void *addr);

void add_page (void *addr, void *data, enum page_status status);

void remove_page (void *addr);

void pagetable_destroy (void);

#endif /* vm/page.h */
