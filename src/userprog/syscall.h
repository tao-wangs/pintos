#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct fd_map {
  int fd;
  struct file *fp;
  struct list_elem *elem; 
};

void syscall_init (void);

#endif /* userprog/syscall.h */
