#include "vm/locklist.h"

void
locklist_init (struct locklist *list)
{
  elem_init (&list->head);
  elem_init (&list->tail);
  list->head.prev = NULL;
  list->head.next = &list->tail;
  list->tail.prev = &list->head;
  list->tail.next = NULL;
}

void
elem_init (struct locklist_elem *e)
{
  lock_init (&e->lock); 
}

/*
Start with no locks
End with HEAD A B
If A is tail do not acquire B
*/
struct locklist_elem *
locklist_begin (struct locklist *list)
{
  lock_acquire (&list->head.lock); 
  lock_acquire (&list->head.next->lock);
  if (list->head.next != &list->tail)
    lock_acquire (&list->head.next->next->lock);
  return list->head.next;
}


/*
Start with locks A B C
End with locks B C D
If C is end of list do not acquire D
*/
struct locklist_elem *
locklist_next (struct locklist_elem *e)
{
  lock_release (&e->prev->lock);
  if (e->next->next != NULL)
    lock_acquire (&e->next->next->lock);
  return e->next;
}

struct locklist_elem *
locklist_end (struct locklist *list)
{
  return &list->tail;
}

/*
Start with no locks
End with no locks
Not sure if we should lock e?
*/
void
locklist_push_front (struct locklist *list, struct locklist_elem *e)
{
  lock_acquire (&list->head.lock);
  lock_acquire (&list->head.next->lock);
  struct locklist_elem *next = list->head.next;
  list->head.next = e;
  e->prev = &list->head;
  e->next = next;
  next->prev = e;
  lock_release (&list->head.lock);
  lock_release (&next->lock); 
}

/*
Start with no locks
End with no locks
Not sure if we should lock e?
*/
void
locklist_push_back (struct locklist *list, struct locklist_elem *e)
{
  lock_acquire (&list->tail.prev->lock);
  lock_acquire (&list->tail.lock);
  struct locklist_elem *prev = list->tail.prev;
  prev->next = e;
  e->prev = prev;
  e->next = &list->tail;
  list->tail.prev = e;
  lock_release (&prev->lock); 
  lock_release (&list->tail.lock);
}

/*
Start with locks A B C
End with locks A B C D
B removed from list, you must remember to release B when you are done!
If C is tail do not acquire D
*/
struct locklist_elem *
locklist_remove (struct locklist_elem *e)
{
  if (e->next->next != NULL)
    lock_acquire (&e->next->next->lock); 
  struct locklist_elem *prev = e->prev;
  struct locklist_elem *next = e->next;
  prev->next = next;
  next->prev = prev;
  return next;
}

/*
Start with no locks, list elems HEAD A B ...
End with no locks, list elems HEAD B ...
Undefined behaviour if list is empty.
*/
struct locklist_elem *
locklist_pop_front (struct locklist *list)
{
  lock_acquire (&list->head.lock);
  lock_acquire (&list->head.next->lock);
  lock_acquire (&list->head.next->next->lock);
  struct locklist_elem *first = list->head.next;
  struct locklist_elem *second = first->next;
  list->head.next = second;
  second->prev = &list->head;
  lock_release (&list->head.lock);
  lock_release (&first->lock);
  lock_release (&second->lock);
  return first;
}

/*
Start with no locks, list elems ... A B TAIL
End with no locks, list elems ... B TAIL
Undefined behaviour if list is empty.
*/
struct locklist_elem *
locklist_pop_back (struct locklist *list)
{
  lock_acquire (&list->tail.prev->prev->lock);
  lock_acquire (&list->tail.prev->lock);
  lock_acquire (&list->tail.lock);
  struct locklist_elem *first = list->tail.prev->prev;
  struct locklist_elem *second = list->tail.prev;
  first->next = &list->tail;
  list->tail.prev = first;
  lock_release (&first->lock);
  lock_release (&second->lock);
  lock_release (&list->tail.lock);
  return second;
}

/*
Iterates without locking
No guarantee that this is correct by the time it returns!
*/
size_t
locklist_size (struct locklist *list)
{
  int i = 0;
  for (struct locklist_elem *e = list->head.next;
       e != &list->tail;
       e = e->next)
  {
    i++;
  }
  return i;
}

/*
Does not lock.
To guarantee correctness lock list->head before calling.
*/
bool
locklist_empty (struct locklist *list)
{
  return list->head.next == &list->tail;
}
