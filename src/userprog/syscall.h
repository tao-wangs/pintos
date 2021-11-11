#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct fd_map {
  int fd;
  struct file *fp;
};

void syscall_init (void);

#endif /* userprog/syscall.h */
