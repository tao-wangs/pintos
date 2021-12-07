#include "threads/threadtable.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include <stdio.h>
 
static unsigned 
thread_hash (const struct hash_elem *e, void *aux UNUSED)
{
  struct threadtable_elem* elem = hash_entry (e, struct threadtable_elem, elem);
  return hash_int(elem->tid);
}

static bool
thread_less (const struct hash_elem *a,
             const struct hash_elem *b,
             void *aux UNUSED)
{
  return hash_entry (a, struct threadtable_elem, elem)->tid
       < hash_entry (b, struct threadtable_elem, elem)->tid;
}

/* Acquire thread table lock. */
void 
threadtable_acquire (struct threadtable *table)
{
  lock_acquire (&table->lock);
}

/* Release thread table lock. */
void
threadtable_release (struct threadtable *table)
{
  lock_release (&table->lock);
}

/* Initialise thread table. */
struct threadtable *
threadtable_init (void)
{
  struct threadtable *table = malloc (sizeof (struct threadtable));
  if (!table)
    return NULL;
  hash_init (&table->table, thread_hash, thread_less, NULL); 
  lock_init (&table->lock);
  table->refs = 1;
  return table;
}

/* Destroy thread table elem, freeing its resources. */
static void
threadtable_elem_destroy (struct hash_elem *e, void *aux UNUSED)
{
  free (hash_entry (e, struct threadtable_elem, elem));
}

/* Destroy thread table, freeing its resources. */
void
threadtable_destroy (struct threadtable *table)
{
  hash_destroy (&table->table, threadtable_elem_destroy);
  lock_release (&table->lock);
  free (table);
}

/* Gets the threadtable_elem * associated with the given tid from the hashtable.
Returns NULL if missing. */
struct threadtable_elem *
find (struct threadtable *table, int tid)
{
  struct threadtable_elem temp;
  temp.tid = tid;
  struct hash_elem *e = hash_find (&table->table, &temp.elem);
  if (!e)
    return NULL;
  return hash_entry(e, struct threadtable_elem, elem);
}

/* Determines if the thread child_tid is a child of the thread parent_tid. */
bool
isChild (struct threadtable *table, int parent_tid, int child_tid)
{
  lock_acquire (&table->lock);
  struct threadtable_elem *e = find (table, child_tid);
  lock_release (&table->lock);
  return e != NULL && e->parent_tid == parent_tid;
}

/* Adds a new thread to the threadtable.
   Initialises its members to default values. */
struct threadtable_elem*
addThread (struct threadtable *table, int parent_tid, int child_tid)
{
  lock_acquire (&table->lock);
  if (find (table, child_tid))
  {
    lock_release (&table->lock);
    return NULL; 
  }
  struct threadtable_elem *elem = malloc (sizeof(struct threadtable_elem));
  if (!elem)
  {
    lock_release (&table->lock);
    return NULL;
  }
  sema_init (&elem->sema, 0);
  sema_init (&elem->start_sema, 0);
  elem->tid = child_tid;
  elem->parent_tid = parent_tid;
  elem->refs = 2;
  elem->waited = false;
  elem->started = false;
  hash_insert (&table->table, &elem->elem);
  table->refs++;
  lock_release (&table->lock); 
  return elem;
}

/* Reduces refs by 1.
   If it becomes zero, the elem is destroyed. */
static void
decrRefs (struct threadtable *table, struct threadtable_elem *elem)
{
  --elem->refs;
  if (elem->refs == 0)
  {
    hash_delete (&table->table, &elem->elem);
    free (elem);
  }
}

static bool
decrTableRefs (struct threadtable *table)
{
  --table->refs;
  if (table->refs == 0)
  {
    threadtable_destroy (table);
    return false;
  }
  return true;
}

/* Called for each child when the parent exits.
   Decrements refs for the elem associated with the child. */
bool
parentExit (struct threadtable *table, int child_tid)
{
  lock_acquire (&table->lock);
  struct threadtable_elem *e = find (table, child_tid);
  if (!e)
  {
    lock_release (&table->lock);
    return false;
  }
  decrRefs (table, e); 
  lock_release (&table->lock); 
  return true;
}

/* Called when a thread exits.
   Sets the exit status and ups the semaphore.
   Wait should not block after this has been called. */
bool
childExit (struct threadtable *table, int tid, int status)
{
  lock_acquire (&table->lock);
  struct threadtable_elem *e = find (table, tid);
  if (!e)
  {
    lock_release (&table->lock);
    return false;
  }
  e->status = status;
  sema_up (&e->sema);
  decrRefs (table, e); 
  if (decrTableRefs (table))
    lock_release (&table->lock); 
  return true;
}
