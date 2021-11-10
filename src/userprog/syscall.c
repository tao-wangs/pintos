#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"

typedef int pid_t; 

static void syscall_handler (struct intr_frame *);

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

/* Reads 4 bytes at user virtual address UADDR. */
static int
get_int (const uint8_t *uaddr)
{
  int result = 0;
  for (int i = 0; i < 4; ++i)
  {
    int temp = get_user (uaddr + i);
    if (temp == -1)
    {
      return -1;
    }
    result |= temp << (24 - 8 * i);
  }
  return result;
}

static void 
halt (void)
{
  shutdown_power_off ();
}

static void 
exit (int status)
{
  struct thread *cur = thread_current (); 

  printf("%s: exit(%d)\n", cur->name, status);  

  thread_exit();
}

static pid_t 
exec (const char *file)
{
}
static int 
wait (pid_t pid)
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
// length is the size in bytes.
{
  int remaining_length = (int) length;
  int file_size = filesize(fd);
  
  // first checks if fd is set to write to the console
  if (fd == STDOUT_FILENO) {
    // Write to the console all of buffer in one call to putbuf(), at least
    // as long as the size is not bigger than a few hundred bytes.
    if (length <= 300) {
      putbuf((const char *) (buffer), length); // TODO: BREAKUP LARGER BUFFERS
      return (int) length;
    } else {
        putbuf((const char*) (buffer), 300);
        // This will decrement the size of the remaining bytes to be written by 300.
        remaining_length -= 300;
        // As each character is of size 1 byte, so we add 300 to the address of the buffer
        // as we have already printed the first 300 bytes of the buffer to the console.
        write(STDOUT_FILENO, buffer + 300, remaining_length);
    }
    
  }
  
  
  // The expected behaviour is to write as many bytes as possible up to end-of-file
  // 
  
  return file_size;
  
  
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

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t intr =  get_int ((uint8_t *) f->esp);
  switch (intr) {
    case SYS_HALT:
     // halt();
      break;
    case SYS_EXIT:
     // exit();
      break;
    case SYS_EXEC:
     // exec();
      break;
    case SYS_WAIT:
     // wait();
      break;
    case SYS_CREATE:
     // create();
      break;
    case SYS_REMOVE:
     // remove();
      break;
    case SYS_OPEN:
     // open();
      break;
    case SYS_FILESIZE:
     // filesize();
      break;
    case SYS_READ:
     // read();
      break;
    case SYS_WRITE:
     // write();
      break;
    case SYS_SEEK:
     // seek();
      break;
    case SYS_TELL:
     // tell();
      break;
    case SYS_CLOSE:
     // close();
      break;
    default:
      ASSERT(1==0);
  }

  thread_exit ();
}

<<<<<<< HEAD



=======
>>>>>>> 39b61c71a2c5ca2af2fa0492df7e34b900e47e02
