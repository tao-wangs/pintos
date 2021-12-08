#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <list.h>

/* Mapping from files to user's virtual memory */ 
struct m_map {
    int mapid;                  /* Mapping id. */
    void *addr;                 /* Base address in VM of mapping. */
    int page_cnt;               /* Number of pages file maps onto. */
    struct file *fp;            /* File pointer. */
    struct list_elem elem;      /* List elem. */
};

#endif /* vm/mmap.h */