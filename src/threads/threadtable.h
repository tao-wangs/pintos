#ifndef __THREAD_TABLE_H_
#define __THREAD_TABLE_H_

#include <hash.h>
#include <stdbool.h>
#include "threads/synch.h"

struct threadtable_elem {
  struct semaphore sema;
  struct hash_elem elem;
  int tid;
  int parent_tid;
  int status;
  int refs;
  bool waited;
};

struct threadtable {
  struct hash table; 
  struct lock lock;
};

void threadtable_acquire (void);

void threadtable_release (void);

void threadtable_init (void);

void threadtable_destroy (void);

struct threadtable_elem * find (int tid);

bool isChild (int parent_tid, int child_tid);

bool addThread (int parent_tid, int child_tid);

bool parentExit (int child_tid);

bool childExit (int tid, int status);

#endif
