#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

static void
halt (void)
{

}

static void
exit (int status)
{

}

/* Tasks 2 and later. */
static void 
halt (void)
{
}
static void 
exit (int status)
{
}
static pid_t 
exec (const char *file)
{
}
static int 
wait (pid_t)
{
}
static bool 
create (const char *file, unsigned initial_size)
{
}
static bool 
remove (const char *file)
{
}
static int 
open (const char *file)
{
}
static int 
filesize (int fd)
{
}
static int 
read (int fd, void *buffer, unsigned length)
{
}
static int 
write (int fd, const void *buffer, unsigned length)
{
}
static void 
seek (int fd, unsigned position)
{
}
static unsigned 
tell (int fd)
{
}
static void 
close (int fd)
{
}

#endif /* lib/user/syscall.h */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t intr =  *((uint32_t *) f->esp);
  switch (intr):
	case SYS_HALT:
	  halt();

    
  thread_exit ();
}




