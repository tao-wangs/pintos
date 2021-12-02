#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <hash.h>
#include "filesys/off_t.h"
#include "threads/synch.h"

enum page_status
{
   FRAME, 
   SWAP, 
   FILE_SYS,
   ZERO
};

struct file_data
{
  struct file *file;
  off_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;
};

struct page
{
  struct hash_elem elem;
  void *addr;
  enum page_status status;
  void *data;
  struct thread *t;
};

struct page_table
{
  struct hash table;
  struct lock lock;
};

/*static unsigned
page_hash (const struct hash_elem *e, void *aux UNUSED);

static bool
page_less (const struct hash_elem *a,
           const struct hash_elem *b,
           void *aux UNUSED);
*/

struct page_table *pagetable_init (void);

struct page *locate_page (void *addr, struct page_table *page_table);

void add_page (void *addr, void *data, enum page_status status, struct page_table *page_table);

void remove_page (void *addr, struct page_table *page_table);

void pagetable_destroy (struct page_table *page_table);

#endif /* vm/page.h */
