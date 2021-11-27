#ifndef __THREAD_TABLE_H_
#define __THREAD_TABLE_H_

#include <hash.h>
#include <stdbool.h>
#include "threads/synch.h"

struct threadtable_elem {
  struct semaphore sema;            /* Synchronises wait with child exit */ 
  struct semaphore start_sema;      /* Synchronises execute with child start */
  struct hash_elem elem;            /* Used to store the elem in the hashtable */
  struct list_elem lst_elem;        /* Used to store the elem in the child list */
  int tid;                          /* tid of the child thread */
  int parent_tid;                   /* tid of the parent thread */
  int status;                       /* exit status of child thread */
  int refs;                         /* counts references to elem */
  bool waited;                      /* has wait been called successfully */
  bool started;                     /* did the program start correctly */
};

struct threadtable {
  struct hash table;        /* Stores elems in a hashtable for fast lookup */
  struct lock lock;         /* Synchronises access to threadtable */
  int refs;
};

void threadtable_acquire (struct threadtable *);

void threadtable_release (struct threadtable *);

struct threadtable *threadtable_init (void);

void threadtable_destroy (struct threadtable *);

struct threadtable_elem * find (struct threadtable *, int tid);

bool isChild (struct threadtable *, int parent_tid, int child_tid);

struct threadtable_elem * addThread (struct threadtable *, int parent_tid, int child_tid);

bool parentExit (struct threadtable *, int child_tid);

bool childExit (struct threadtable *, int tid, int status);

#endif
