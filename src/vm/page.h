#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>

enum page_status
{
  FRAME, SWAP, FILESYS
};

struct page
{
  struct hash_elem elem;
  void *addr;
  enum page_status status;
  void *data;
};

void pagetable_init ();

struct page *locate_page (void *addr);

void add_page (void *addr, void *data, enum page_status status);

void remove_page (void *addr);
#endif /* vm/page.h */
