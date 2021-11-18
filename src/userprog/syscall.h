#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h> 

/* Mapping from file descriptors to its file */
struct fd_map {
  int fd;                                 /* File descriptor */
  struct file *fp;                        /* File pointer matching fd */
  struct list_elem elem;                  /* List elem */
};

int get_user (const uint8_t *uaddr);
bool put_user (uint8_t *udst, uint8_t byte);
int get_int (const uint8_t *uddr);

void syscall_init (void);
void exit (int status);

#endif /* userprog/syscall.h */
