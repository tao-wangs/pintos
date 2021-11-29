#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <list.h>

struct frame {
  struct list_elem elem;
  void *page;
  bool accessed;
};

struct frametable {
  struct list frames;
};

void frametable_init (void);

void frametable_free (void);

void alloc_frame (void *page);

void free_frame (void *page);

#endif /* vm/frame.h */
