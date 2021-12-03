#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

struct frame {
  struct list_elem elem;
  void *page;
  void *kPage;
  bool accessed;
  //? bool writable;
};

struct frametable {
  struct list frames;
};

void frametable_init (void);

void frametable_free (void);

struct frame *alloc_frame (void *page);

void free_frame (void *page);

#endif /* vm/frame.h */
