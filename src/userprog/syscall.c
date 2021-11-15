#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

typedef int pid_t; 

static int fd_incr = 2;

static void syscall_handler (struct intr_frame *);
struct file *get_corresponding_file (int fd);
static void halt(void);
static void exit(int status);
static pid_t exec(const char *file);
static int wait(pid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned length);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);
static int get_user (const uint8_t *uddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static int get_int (const uint8_t *uddr);

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
    int temp = get_user (uaddr + (3 - i));
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
  return process_execute(file);
}

static int 
wait (pid_t pid)
{
}

static bool 
create (const char *file, unsigned initial_size)
{
  int32_t file_size = (int32_t) initial_size;
  return filesys_create(file, file_size);
}

static bool 
remove (const char *file)
{
  return filesys_remove(file);
}
static int 
open (const char *file)
{
  
  struct thread *current_thread = thread_current();
  struct list *files = &current_thread->file_list;
  struct file *fp = filesys_open(file);
  struct list_elem elem;

  struct list_elem *e;

  // maintains the invariant that a file cannot be opened by a thread more than once simultaneously
  for(e = list_begin(files); e != list_end(files); e = list_next(e)){
    struct fd_map *current_fp = list_entry(e, struct fd_map, elem);
    if(current_fp->fp == fp){
      file_close(fp);
      break; //can break as soon as we find that fp is already open by the current thread because of our invariant
    }
  }

  if (fp == NULL){
    return -1;
  }

  int current_ticks = ++fd_incr; //lock this

  struct fd_map current_fd_map;
  current_fd_map.fp = fp;
  current_fd_map.elem = elem;
  current_fd_map.fd = current_ticks + 2;
  list_push_back(&(current_thread->file_list), &elem);

  return current_fd_map.fd;

  /*struct file *file_ptr = get_corresponding_file(fd);

  struct inode *inode_ptr = file_get_inode(inode_ptr);
  struct file *return_ptr = file_open(inode_ptr);
  
  // Returns -1 if the file could not be open, in which case
  // file_open will return a null pointer.
  if(return_ptr == NULL) {
    return -1;
  } else {
  // This is not a proper implementation that we should use, instead
  // it is a temporary fix. See userprog.texi in doc directory at line
  // 981 for details regarding casting a struct file * to get a file descriptor
  // which is of type int. Adding the thread identifier of the current thread
  // ensures that when a file is opened by different processes, each open 
  // returns a new file descriptor. Also see #177 on EdStem for further details. 
    return (int) return_ptr + current_thread->tid_t;
  }*/
}

// This should return a pointer to the file, from its file descriptor.
struct file *
get_corresponding_file (int fd) {
  struct list *files = &thread_current()->file_list;

  struct list_elem *e;

  for(e = list_begin(files); e != list_end(files); e = list_next(e)){
    struct fd_map *current_fd_map = list_entry(e, struct fd_map, elem);
    if(current_fd_map->fd == fd){
      return current_fd_map->fp;
    }
  }
  return NULL; //this shouldnt really work, maybe fix

	/*struct thread *current_thread = current_thread();
  int current_tid_t = current_thread->tid_t;
  // Brackets are needed because cast has a higher precedence than subtraction in c.
  struct file *file_ptr = (struct file *) (fd - current_tid_t); 
  */
}

static int 
filesize (int fd)
{
  struct file *file_ptr = get_corresponding_file(fd);
  return file_length(file_ptr);
}

static int 
read (int fd, void *buffer, unsigned length)
{
}

static int 
write (int fd, const void *buffer, unsigned length)
// length is the size in bytes.
{
 
  int file_size = filesize(fd);
  const char* char_buffer = (const char *) buffer;
  
  // This doesn't break up the larger buffers into shorter buffers correctly, as
  // the first recursive call will cause the length value to be returned, and so 
  // we won't iterate through the rest of the buffer.
  
  // first checks if fd is set to write to the console
  if (fd == STDOUT_FILENO) {
    int written_bytes_acc = 0;
    // Write to the console all of buffer in one call to putbuf(), at least
    // as long as the size is not bigger than a few hundred bytes.
    while (length >= 256) {
       putbuf(char_buffer , 256);
       length -= 256;
       written_bytes_acc += 256;
    }
    putbuf(char_buffer, length);

    return written_bytes_acc;
  }

  
  struct file *file_ptr = get_corresponding_file(fd);
  return file_write(file_ptr, char_buffer, length); 
}

static void 
seek (int fd, unsigned position)
{
  
  struct file *file_ptr = get_corresponding_file(fd);
  int32_t new_pos = (int32_t) position;
  file_seek(file_ptr, new_pos);
  
}

static unsigned 
tell (int fd)
{
  struct file *file_ptr = get_corresponding_file(fd);
  int32_t next_byte_pos = file_tell (file_ptr);
  return (uint32_t) next_byte_pos;
}

static void 
close (int fd)
{
  
  struct file *open_file = get_corresponding_file (fd);
  
  struct list *files = &thread_current ()->file_list;

  struct list_elem *e;

  for(e = list_begin (files); e != list_end (files); e = list_next (e)){
    struct fd_map *current_fd_map = list_entry (e, struct fd_map, elem);
    if (current_fd_map->fd == fd){
      list_remove (&current_fd_map->elem);
    }
  }


  file_close (open_file);
  
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
  void *arg1 = (void *) get_int ((uint8_t *) f->esp + 4);
  void *arg2 = (void *) get_int ((uint8_t *) f->esp + 8);
  void *arg3 = (void *) get_int ((uint8_t *) f->esp + 12);
  printf ("REQUIRED INTERRUPT: %u\n", intr);
  switch (intr) {
    case SYS_HALT:
      halt ();
      break;
    case SYS_EXIT:
      exit ((int) arg1);
      break;
    case SYS_EXEC:
      f->eax = exec ((const char *) arg1);
      break;
    case SYS_WAIT:
      f->eax = wait ((pid_t) arg1);
      break;
    case SYS_CREATE:
      f->eax = create ((const char *) arg1, (unsigned) arg2);
      break;
    case SYS_REMOVE:
      f->eax = remove ((const char *) arg1);
      break;
    case SYS_OPEN:
      f->eax = open ((const char *) arg1);
      break;
    case SYS_FILESIZE:
      f->eax = filesize ((int) arg1);
      break;
    case SYS_READ:
      f->eax = read ((int) arg1, arg2, (unsigned int) arg3); 
      break;
    case SYS_WRITE:
      f->eax = write ((int) arg1, arg2, (unsigned int) arg3);
      break;
    case SYS_SEEK:
      seek ((int) arg1, (unsigned int) arg2);
      break;
    case SYS_TELL:
      f->eax = tell ((int) arg1);
      break;
    case SYS_CLOSE:
      close ((int) arg2);
      break;
    default:
      printf ("System call number not recognised\n");
      ASSERT(1==0);
  }

  thread_exit ();
}

