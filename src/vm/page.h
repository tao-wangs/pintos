#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <hash.h>
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "threads/synch.h"
#include "vm/locklist.h"

/* Information about what is in the page. */
enum page_status
{
   FRAME, 
   SWAP, 
   FILE_SYS,
   ZERO
};

/* File data. */
struct file_data
{
  struct file *file;         /* File pointer. */
  off_t ofs;                 /* Offset.       */
  uint32_t read_bytes;       /* Number of read_bytes. */
  uint32_t zero_bytes;       /* Number of zero_bytes (PGSIZE - read_bytes). */
};

/* Supplemental page, per thread. */
struct page
{
  struct hash_elem elem;          /* Hash elem. */ 
  void *addr;                     /* Page pointer. */
  enum page_status status;        /* Page status. */
  void *data;                     /* Data depending on page_status. */
  struct thread *t;               /* Thread owning the supplemental page table. */
  bool writable;                  /* Writable. */
  struct inode *node;             /* Inode. */
  struct locklist_elem page_elem; /* Used to store page in frame page_list. */
  struct list_elem swap_elem;     /* Used to store page in swap page_list. */
};

/* Supplemental page table, synchronised with a lock. */
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
