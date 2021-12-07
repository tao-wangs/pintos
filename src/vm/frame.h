#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "filesys/file.h"
#include "page.h"

struct frame {
  int fid;
  struct list_elem elem;
  void *page;
  void *kPage;
  bool accessed;
  bool writable;
  int num_refs;
  struct inode *file_node;
  struct list page_list;
  struct lock lock;
};

struct frametable {
  struct list frames;
};

void frametable_init (void);

void frametable_free (void);

struct frame *alloc_frame (struct page *page, bool writable, struct inode *exe, bool *shared);

struct frame *locate_frame (void *page, struct inode *node); 

struct frame *find_free_frame ();

void free_frame (void *kpage);

void size();

#endif /* vm/frame.h */
