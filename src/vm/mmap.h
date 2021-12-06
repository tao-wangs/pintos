#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <list.h>

struct m_map {
    int mid;                    /* Mapping id */
    void *addr;                 /* Base address in VM of mapping */
    int page_cnt;               /* Number of pages file maps onto */
    struct file *fp;            /* File pointer */
    struct list_elem elem;      /* List elem */
    
    uint32_t read_bytes; 
};

#endif /* vm/mmap.h */