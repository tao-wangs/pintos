#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <hash.h>
#include "filesys/off_t.h"
#include "filesys/file.h"
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
};

struct page
{
  struct hash_elem elem;
  void *addr;
  enum page_status status;
  void *data;
  struct thread *t;
  bool writable;
  struct inode *node;
  struct list_elem page_elem;
  struct list_elem swap_elem;
};

struct page_table
{
  struct hash table;
  struct lock lock;
};

struct page_table *pagetable_init (void);

struct page *locate_page (void *addr, struct page_table *page_table);

void add_page (void *addr, void *data, enum page_status status, struct page_table *page_table, bool writable);

void remove_page (void *addr, struct page_table *page_table);

void pagetable_destroy (struct page_table *page_table);

#endif /* vm/page.h */
