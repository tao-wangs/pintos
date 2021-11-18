#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h> 

struct fd_map {
  int fd;
  struct file *fp;
  struct list_elem elem; 
};

int get_user(const uint8_t *uaddr);
bool put_user (uint8_t *udst, uint8_t byte);
int get_int (const uint8_t *uddr);

void syscall_init (void);
void exit (int status);

#endif /* userprog/syscall.h */
