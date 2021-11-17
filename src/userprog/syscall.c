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
#include "threads/threadtable.h"
#include "threads/malloc.h"
#include <string.h>

extern struct lock filesystem_lock;
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
  //checks uaddr is below PHYS_BASE
  if (!is_user_vaddr(uaddr)) {
    exit(-1);
  } 
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
  //checks udst is below PHYS_BASE
  if (!is_user_vaddr(udst)) {
    exit(-1);
  } 
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
  childExit (cur->tid, status);

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
  return process_wait (pid);
}

static bool
filename_valid (const char *file)
{
  if (!file) {
    exit(-1);
  }
  char buffer[15];
  bool valid = false;
  for (int i = 0; i < 15; ++i) {
    buffer[i] = get_user(file + i);
    if (buffer[i] == -1)
      exit (-1);
    else if (buffer[i] == 0)
    {
      valid = true;
      break; 
    }
  } 
  return valid;
}

static bool 
create (const char *file, unsigned initial_size)
{ 
  if (!filename_valid (file))
    return false;
  lock_acquire(&filesystem_lock);
  int32_t file_size = (int32_t) initial_size;
  bool return_value = filesys_create(file, file_size);
  lock_release(&filesystem_lock);
  return return_value;
}

static bool 
remove (const char *file)
{
  lock_acquire(&filesystem_lock);
  bool return_value = filesys_remove(file);
  lock_release(&filesystem_lock);
  return return_value;
}
static int 
open (const char *file)
{ 
  //printf("Opening file: %s\n", file);
  if (!filename_valid (file))
    exit (-1);
  
  lock_acquire(&filesystem_lock);
  
  struct file *fp = filesys_open(file);

  if (fp == NULL){ 
    lock_release(&filesystem_lock);
    return -1;
  }
  
  int current_ticks = ++fd_incr; 

  lock_release(&filesystem_lock);

  struct fd_map fd_map;

  fd_map.fp = fp;
  fd_map.fd = current_ticks + 2;

  list_push_back(&(thread_current ()->file_list), &fd_map.elem);

  return fd_map.fd;
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
}

static int 
filesize (int fd)
{
  lock_acquire(&filesystem_lock);
  struct file *file_ptr = get_corresponding_file(fd);
  if (!file_ptr) {
    lock_release(&filesystem_lock);
    return -1;
  }
  int return_value = file_length(file_ptr);
  lock_release(&filesystem_lock);
  return return_value;
}

static int 
read (int fd, void *buffer, unsigned length)
{
  if (!length) 
    return 0;

  int bytesRead = 0;
  char *buff = malloc (length);
  if (!buff)
    exit (-1);
  if (fd == STDIN_FILENO) {
    for (uint32_t i = 0; i < length; ++i) {
      buff[i] = input_getc (); 
      bytesRead++; 
    }
  }
  else
  { 
    lock_acquire(&filesystem_lock);
    struct file *file_ptr = get_corresponding_file (fd);
    if (!file_ptr) {
      lock_release(&filesystem_lock);
      free (buff);
      return -1;
    }

    bytesRead = file_read (file_ptr, buff, length);
    
    lock_release(&filesystem_lock);
  }
  for (int i = 0; i < bytesRead; ++i)
  {
    if (!put_user(buffer + i, buff[i]))
    {
      free (buff);
      exit (-1);
    }
  }
  free (buff);
  return bytesRead; 
}

static int 
write (int fd, const void *buffer, unsigned length)
// length is the size in bytes.
{
  if (!buffer)
    exit (-1);
  if (!length)
    return 0;
  char *buff = malloc (length);
  if (!buff)
    exit (-1);
  if (is_user_vaddr (buffer))
  {
    for (int i = 0; i < length; ++i)
    {
      buff[i] = get_user (buffer + i);
      if (buff[i] == -1)
      {
        free (buff);
        exit (-1);
      }
    }
  }
  else
  {
    memcpy (buff, buffer, length);
  }
  
  // This doesn't break up the larger buffers into shorter buffers correctly, as
  // the first recursive call will cause the length value to be returned, and so 
  // we won't iterate through the rest of the buffer.
  
  // first checks if fd is set to write to the console
  if (fd == STDOUT_FILENO) {
    int written_bytes_acc = 0;
    // Write to the console all of buffer in one call to putbuf(), at least
    // as long as the size is not bigger than a few hundred bytes.
    while (length >= 256) {
       putbuf(buff , 256);
       length -= 256;
       written_bytes_acc += 256;
    }
    putbuf(buff, length);
    free (buff);
    return written_bytes_acc += length;
  }

  lock_acquire(&filesystem_lock);  
  struct file *file_ptr = get_corresponding_file(fd);
  if (!file_ptr) {
    lock_release(&filesystem_lock);
    free (buff);
    return -1;
  }
  int return_value = file_write(file_ptr, buff, length); 
  lock_release(&filesystem_lock);
  free (buff);
  return return_value;
}

static void 
seek (int fd, unsigned position)
{
  lock_acquire(&filesystem_lock);
  struct file *file_ptr = get_corresponding_file(fd);
  int32_t new_pos = (int32_t) position;
  file_seek(file_ptr, new_pos);
  lock_release(&filesystem_lock);
}

static unsigned 
tell (int fd)
{
  lock_acquire(&filesystem_lock);
  struct file *file_ptr = get_corresponding_file(fd);
  int32_t next_byte_pos = file_tell (file_ptr);
  lock_release(&filesystem_lock);
  return (uint32_t) next_byte_pos;
}

static void 
close (int fd)
{
  
  lock_acquire(&filesystem_lock);
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
  
  lock_release(&filesystem_lock);
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
  //void *get_int ((uint8_t *) f->esp + 4) = (void *) get_int ((uint8_t *) f->esp + 4);
  //void *get_int ((uint8_t *) f->esp + 8) = (void *) get_int ((uint8_t *) f->esp + 8);
  //void *get_int ((uint8_t *) f->esp + 12) = (void *) get_int ((uint8_t *) f->esp + 12);
  //printf ("REQUIRED INTERRUPT: %u\n", intr);
  switch (intr) {
    case SYS_HALT:
      halt ();
      break;
    case SYS_EXIT:
      exit ((int) get_int ((uint8_t *) f->esp + 4));
      break;
    case SYS_EXEC:
      f->eax = exec ((const char *) get_int ((uint8_t *) f->esp + 4));
      break;
    case SYS_WAIT:
      f->eax = wait ((pid_t) get_int ((uint8_t *) f->esp + 4));
      break;
    case SYS_CREATE:
      f->eax = create ((const char *) get_int ((uint8_t *) f->esp + 4), (unsigned) get_int ((uint8_t *) f->esp + 8));
      break;
    case SYS_REMOVE:
      f->eax = remove ((const char *) get_int ((uint8_t *) f->esp + 4));
      break;
    case SYS_OPEN:
      f->eax = open ((const char *) get_int ((uint8_t *) f->esp + 4));
      break;
    case SYS_FILESIZE:
      f->eax = filesize ((int) get_int ((uint8_t *) f->esp + 4));
      break;
    case SYS_READ:
      f->eax = read ((int) get_int ((uint8_t *) f->esp + 4), get_int ((uint8_t *) f->esp + 8), (unsigned int) get_int ((uint8_t *) f->esp + 12)); 
      break;
    case SYS_WRITE:
      f->eax = write ((int) get_int ((uint8_t *) f->esp + 4), get_int ((uint8_t *) f->esp + 8), (unsigned int) get_int ((uint8_t *) f->esp + 12));
      break;
    case SYS_SEEK:
      seek ((int) get_int ((uint8_t *) f->esp + 4), (unsigned int) get_int ((uint8_t *) f->esp + 8));
      break;
    case SYS_TELL:
      f->eax = tell ((int) get_int ((uint8_t *) f->esp + 4));
      break;
    case SYS_CLOSE:
      close ((int) get_int ((uint8_t *) f->esp + 8));
      break;
    default:
      //printf ("System call number not recognised\n");
      exit(-1);
      //ASSERT(1==0);
  }
}

