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

/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user (const uint8_t *uaddr)
{
  ASSERT (is_user_vaddr(uaddr)); //checks uaddr is below PHYS_BASE
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
  : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{ 
  ASSERT (is_user_vaddr(udst)); //checks udst is below PHYS_BASE
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
  : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}



