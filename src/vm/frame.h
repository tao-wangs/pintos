#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>
#include "filesys/file.h"

struct frame {
  struct list_elem elem;      /* List elem. */
  void *page;                 /* User address. */
  void *kPage;                /* Frame's address in user pool. */
  bool accessed;              /* Accessed. */
  bool writable;              /* Writable. */
  int num_refs;               /* Number of references. */
  struct inode *file_node;    /* Inode. */
};

/* List of frames */
struct frametable {
  struct list frames;
};

void frametable_init (void);

void frametable_free (void);

struct frame *alloc_frame (void *page, bool writable, struct inode *exe, bool *shared);

struct frame *locate_frame (void *page, struct inode *node); 

void free_frame (void *kpage);

#endif /* vm/frame.h */
