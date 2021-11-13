#include "threads/threadtable.h"
#include "threads/malloc.h"

struct threadtable table;

static unsigned 
thread_hash (const struct hash_elem *e, void *aux)
{
  struct threadtable_elem* elem = hash_entry (e, struct threadtable_elem, elem);
  return hash_int(elem->tid);
}

static bool
thread_less (const struct hash_elem *a,
             const struct hash_elem *b,
             void *aux)
{
  return hash_entry (a, struct threadtable_elem, elem)->tid
       < hash_entry (b, struct threadtable_elem, elem)->tid;
}

void 
threadtable_acquire (void)
{
  lock_acquire (&table.lock);
}

void
threadtable_release (void)
{
  lock_release (&table.lock);
}

void
threadtable_init (void)
{
  hash_init (&table.table, thread_hash, thread_less, NULL); 
  lock_init (&table.lock);
}

static void
threadtable_elem_destroy (struct hash_elem *e, void *aux)
{
  free (hash_entry (e, struct threadtable_elem, elem));
}

void
threadtable_destroy (void)
{
  hash_destroy (&table.table, threadtable_elem_destroy);
}

struct threadtable_elem *
find (int tid)
{
  struct threadtable_elem temp;
  temp.tid = tid;
  return hash_entry(hash_find (&table.table, &temp.elem),
                    struct threadtable_elem, elem);
}

bool
isChild (int parent_tid, int child_tid)
{
  lock_acquire (&table.lock);
  struct threadtable_elem *e = find (child_tid);
  lock_release (&table.lock);
  return e != NULL && e->parent_tid == parent_tid;
}

bool
addThread (int parent_tid, int child_tid)
{
  lock_acquire (&table.lock);
  if (find (child_tid))
  {
    lock_release (&table.lock);
    return false; 
  }
  struct threadtable_elem *elem = malloc (sizeof(struct threadtable_elem));
  if (!elem)
  {
    lock_release (&table.lock);
    return false;
  }
  sema_init (&elem->sema, 0);
  elem->tid = child_tid;
  elem->parent_tid = parent_tid;
  elem->refs = 2;
  elem->waited = false;
  hash_insert (&table.table, &elem->elem);
  lock_release (&table.lock); 
  return true;
}

static void
decrRefs (struct threadtable_elem *elem)
{
  --elem->refs;
  if (elem->refs == 0)
  {
    hash_delete (&table.table, &elem->elem);
    free (elem);
  }
}

bool
parentExit (int child_tid)
{
  lock_acquire (&table.lock);
  struct threadtable_elem *e = find (child_tid);
  if (!e)
  {
    lock_release (&table.lock);
    return false;
  }
  decrRefs (e); 
  lock_release (&table.lock); 
  return true;
}

bool
childExit (int tid, int status)
{
  lock_acquire (&table.lock);
  struct threadtable_elem *e = find (tid);
  if (!e)
  {
    lock_release (&table.lock);
    return false;
  }
  e->status = status;
  decrRefs (e); 
  lock_release (&table.lock); 
  return true;
}
