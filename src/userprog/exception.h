#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

#include <stdbool.h>

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

#define MAX_STACK_SIZE 8388608  /* 8MB in bytes, which is the maximum size of the stack. */
void exception_init (void);
void exception_print_stats (void);

bool is_a_stack_access (bool write, void *fault_addr);
void grow_the_stack (void *fault_addr);

#endif /* userprog/exception.h */
