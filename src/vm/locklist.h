#ifndef __LOCKLIST_H
#define __LOCKLIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "threads/synch.h"

/* List element. */
struct locklist_elem 
  {
    struct locklist_elem *prev;     /* Previous list element. */
    struct locklist_elem *next;     /* Next list element. */
    struct lock lock;
  };

/* List. */
struct locklist 
  {
    struct locklist_elem head;      /* List head. */
    struct locklist_elem tail;      /* List tail. */
  };

void locklist_init (struct locklist *);
void elem_init (struct locklist_elem *);

/* List traversal. */
struct locklist_elem *locklist_begin (struct locklist *);
struct locklist_elem *locklist_next (struct locklist_elem *);
struct locklist_elem *locklist_end (struct locklist *);

void locklist_push_front (struct locklist *, struct locklist_elem *);
void locklist_push_back (struct locklist *, struct locklist_elem *);

/* List removal. */
struct locklist_elem *locklist_remove (struct locklist_elem *);
struct locklist_elem *locklist_pop_front (struct locklist *);
struct locklist_elem *locklist_pop_back (struct locklist *);

size_t locklist_size (struct locklist *);
bool locklist_empty (struct locklist *);

#endif 
