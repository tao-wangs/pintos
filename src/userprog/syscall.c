#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/threadtable.h"
#include "threads/malloc.h"
#include "vm/mmap.h"
#include <string.h>

typedef int pid_t; 
typedef int mapid_t;

/* File system lock */
struct lock filesystem_lock;

static void syscall_handler (struct intr_frame *);
struct file *get_corresponding_file (int fd);
static void halt(void);
void exit(int status);
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

static mapid_t mmap (int fd, void *addr);
static void munmap (mapid_t mapid_t);
static struct m_map *find_mmap (mapid_t mapid);
static void munmap_all (void);

static void *first_arg (struct intr_frame *f);
static void *second_arg (struct intr_frame *f);
static void *third_arg (struct intr_frame *f);

/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
int
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
bool
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
int
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

/* Checks if file_name is valid. */
static bool
filename_valid (const char *file)
{
  if (!file) {
    exit (-1);
  }
  char buffer[15];
  bool valid = false;
  for (int i = 0; i < 15; ++i) {
    buffer[i] = get_user ((const uint8_t *) file + i);
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

// This should return a pointer to the file, from its file descriptor.
struct file *
get_corresponding_file (int fd) 
{
  struct list *files = &thread_current ()->file_list;

  struct list_elem *e;

  for (e = list_begin (files); e != list_end (files); e = list_next (e)){
    struct fd_map *current_fd_map = list_entry (e, struct fd_map, elem);
    if (current_fd_map->fd == fd){
      return current_fd_map->fp;
    }
  }

  return NULL; 
}

/* Terminates Pintos by calling shutdown_power_off(). */
static void 
halt (void)
{
  shutdown_power_off ();
}

/* Terminates current user process, notifying its child processes.
   Prints to console process termination message. */
void 
exit (int status)
{
  struct thread *cur = thread_current (); 

  printf ("%s: exit(%d)\n", cur->name, status);  
  
  childExit (cur->parent_table, cur->tid, status);

  munmap_all ();
  
  thread_exit ();
}

/* Executes the program with the file name file.
 * Returns the thread/process id. */
static pid_t 
exec (const char *file)
{
  return process_execute (file);
}

/* Calls process wait */
static int 
wait (pid_t pid)
{
  return process_wait (pid);
}

/* Creates a new file called file initially initial size bytes in size. 
   Returns true if successful, false otherwise. */
static bool 
create (const char *file, unsigned initial_size)
{ 
  if (!filename_valid (file))
    return false;
  
  lock_acquire (&filesystem_lock);
  
  int32_t file_size = (int32_t) initial_size;
  bool success = filesys_create (file, file_size);
  
  lock_release (&filesystem_lock);
  return success;
}

/* Deletes the file called file. Returns true if successful, false otherwise. */
static bool 
remove (const char *file)
{
  lock_acquire (&filesystem_lock);
  
  bool success = filesys_remove (file);
  
  lock_release (&filesystem_lock);
  
  return success;
}

/* Opens the file called file. Returns a nonnegative integer handle fd,
   or -1 if file could not be opened. */
static int 
open (const char *file)
{ 
  if (!filename_valid (file))
    exit (-1);
  
  lock_acquire (&filesystem_lock);
  
  struct file *fp = filesys_open (file);

  if (fp == NULL){ 
    lock_release (&filesystem_lock);
    return -1;
  }
  
  int current_ticks = thread_current ()->fd_incr++;
  lock_release(&filesystem_lock);

  struct fd_map *fd_map = malloc (sizeof (fd_map));
  if (!fd_map)
    exit (-1);

  fd_map->fp = fp;
  fd_map->fd = current_ticks;

  list_push_back(&thread_current ()->file_list, &fd_map->elem);

  return fd_map->fd;
}

/* Returns size of file open as fd in bytes */
static int 
filesize (int fd)
{
  lock_acquire (&filesystem_lock);
  
  struct file *file_ptr = get_corresponding_file (fd);
  
  if (!file_ptr) {
    lock_release (&filesystem_lock);
    return -1;
  }

  int length = file_length (file_ptr);
  
  lock_release (&filesystem_lock);
  
  return length;
}

/* reads size bytes from the file open as fd into buffer
 * returns number of bytes read */
static int 
read (int fd, void *buffer, unsigned length)
{
  if (!length) {
    return 0;
  }

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
    lock_acquire (&filesystem_lock);
    
    struct file *file_ptr = get_corresponding_file (fd);
    if (!file_ptr) {
      lock_release (&filesystem_lock);
      free (buff);
      return -1;
    }

    bytesRead = file_read (file_ptr, buff, length);
    
    lock_release (&filesystem_lock);
  }
  for (int i = 0; i < bytesRead; ++i)
  {
    if (!put_user (buffer + i, buff[i]))
    {
      free (buff);
      exit (-1);
    }
  }
  free (buff);
  
  return bytesRead; 
}

/* Writes to the given fd, breaks up the buffer if the fd is the console.
   Returns number of bytes written. */ 
static int 
write (int fd, const void *buffer, unsigned length)
// length is the size in bytes.
{
  if (!buffer)
    exit (-1);
  if (!length)
    return 0;
  char *buff = malloc (sizeof (char) * length);
  if (!buff)
    exit (-1);
  if (is_user_vaddr (buffer))
  {
    for (unsigned int i = 0; i < length; ++i)
    {
      int result = get_user (buffer + i);
      if (result == -1)
      {
        free (buff);
        exit (-1);
      }
      buff[i] = result;
    }
  }
  else
  {
    memcpy (buff, buffer, length);
  }
  
  // checks if fd is set to write to the console
  if (fd == STDOUT_FILENO) {
    int written_bytes_acc = 0;
    // Write to the console all of buffer in one call to putbuf(), at least
    // as long as the size is not bigger than a few hundred bytes.
    while (length >= 256) {
       putbuf(buff , 256);
       length -= 256;
       written_bytes_acc += 256;
    }
    putbuf (buff, length);
    free (buff);
    return written_bytes_acc += length;
  }

  lock_acquire (&filesystem_lock);  
  
  struct file *file_ptr = get_corresponding_file (fd);

  if (!file_ptr) {
    lock_release (&filesystem_lock);
    free (buff);
    return -1;
  }

  int bytesWritten = file_write (file_ptr, buff, length); 
  
  lock_release (&filesystem_lock);
  
  free (buff);
  
  return bytesWritten;
}

/* Changes the next byte to be read or written in open file fd to position, 
expressed in bytes from the beginning of the file. */
static void 
seek (int fd, unsigned position)
{
  lock_acquire (&filesystem_lock);
  
  struct file *file_ptr = get_corresponding_file (fd);
  int32_t new_pos = (int32_t) position;
  file_seek (file_ptr, new_pos);
  
  lock_release (&filesystem_lock);
}

/* Returns the position of the next byte to be read or written in open file fd, 
expressed in bytes from the beginning of the file. */
static unsigned 
tell (int fd)
{
  lock_acquire (&filesystem_lock);
  
  struct file *file_ptr = get_corresponding_file (fd);
  int32_t next_byte_pos = file_tell (file_ptr);
  
  lock_release (&filesystem_lock);
  
  return (uint32_t) next_byte_pos;
}

/* Closes file descriptor fd. */
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
      free (current_fd_map);
      break;
    }
  }
  file_close (open_file);

  lock_release (&filesystem_lock);
}

/* Maps file fd to process' virtual memory starting at address addr. */
static mapid_t 
mmap (int fd, void *addr)
{ 
  /* Fails if fd 0 or fd 1 or address 0 because cannot they be mapped, address is not page aligned */             
  if (fd == STDIN_FILENO || fd == STDOUT_FILENO || !addr || pg_ofs(addr) != 0) {
    return MMAP_ERROR; 
  }

  struct file *fp = get_corresponding_file (fd);

  if (!fp) {
    return MMAP_ERROR;
  }

  /* Obtain new independent reference to the file */
  struct file *new_fp = file_reopen (fp);

  if (!new_fp) {
    return MMAP_ERROR;
  }

  int offset = 0;

  lock_acquire (&filesystem_lock);

  int remaining_length = file_length (new_fp);
  
  if (!remaining_length) {
    lock_release (&filesystem_lock);
    return MMAP_ERROR;
  }

  int pages = remaining_length/PGSIZE + (remaining_length % PGSIZE != 0);

  /* Check that pending mapping will not overlap existing mappings */ 
  for (int i = 0; i < pages; i++) {
    if (locate_page ((uint8_t *) addr + (i * PGSIZE), thread_current ()->page_table) != NULL) {
      lock_release (&filesystem_lock);
      return MMAP_ERROR;
    }
  }

  /* Add mapping to thread's list of mappings */ 
  struct m_map *mapping = malloc (sizeof (struct m_map));

  if (!mapping) {
    lock_release (&filesystem_lock);
    return MMAP_ERROR; 
  }

  mapping->mapid = thread_current ()->mapid_incr++;
  
  lock_release (&filesystem_lock);

  mapping->addr = addr;
  mapping->fp = new_fp;
  mapping->page_cnt = 0;

  list_push_back (&thread_current ()->mappings, &mapping->elem);

  /* Add pages to supplemental page table */
  while (remaining_length > 0) {
    struct file_data *file_data = malloc (sizeof (file_data));

    if (!file_data) {
      return MMAP_ERROR;
    }

    uint32_t read_bytes = remaining_length < PGSIZE ? remaining_length : PGSIZE;

    file_data->file = new_fp;
    file_data->ofs = offset;
    file_data->read_bytes = read_bytes;
    file_data->zero_bytes = PGSIZE - read_bytes;

    add_page ((uint8_t *) addr + offset, file_data, FILE_SYS, thread_current ()->page_table, true);

    mapping->page_cnt++;

    remaining_length -= file_data->read_bytes;
    offset += file_data->read_bytes;
  }
  
  return mapping->mapid;
}

/* Unmaps file fd from process' virtual memory starting at address addr. 
   Frees mapping from user's list of mappings.  */
static void 
munmap (mapid_t mapid) 
{
  struct m_map *mmap = find_mmap (mapid);

  if (!mmap) {
    return;
  }

  /* Only writes modified pages back to the file */
  for (int i = 0; i < mmap->page_cnt; i++) {
    if (pagedir_is_dirty (thread_current ()->pagedir, (uint8_t *) mmap->addr + PGSIZE * i)) {
      lock_acquire (&filesystem_lock);
      file_write_at (mmap->fp, mmap->addr + PGSIZE * i, PGSIZE, PGSIZE * i);
      lock_release (&filesystem_lock);
    }
    remove_page ((uint8_t *) mmap->addr + PGSIZE * i, thread_current ()->page_table);
  }
  
  list_remove (&mmap->elem);
  free (mmap);
}

/* Locates and returns the mapping within the current process with mapping id mapid */
static struct m_map *
find_mmap (mapid_t mapid) 
{
  struct list_elem *e;
  struct list *mappings = &thread_current ()->mappings;

  for (e = list_begin (mappings); e != list_end (mappings); e = list_next (e)) {
    struct m_map *mmap = list_entry (e, struct m_map, elem);
    if (mmap->mapid == mapid) {
      return mmap;
    }
  }

  return NULL;
} 

/* Unmaps all files for current user process  */
static void
munmap_all (void) {
  struct thread *cur = thread_current ();
  struct list_elem *e = list_begin (&cur->mappings);
  
  while (e != list_end (&cur->mappings)) {
    struct list_elem *next = list_next (e);
    struct m_map *mmap = list_entry (e, struct m_map, elem);
    munmap (mmap->mapid);
    e = next;
  }
}

/* Initialises system call handler and file system lock. */
void
syscall_init (void) 
{
  lock_init (&filesystem_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

// Whenever a user process wants to access some kernel functionality, it invokes a
// system call.
/* Reads syscall number and delegates to its corresponding function. */
static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t intr =  get_int ((uint8_t *) f->esp);
  
   /* You will need to arrange another way, such as saving esp into struct thread 
     on the initial transition from user to kernel mode.
     This is for stack growth in the case in which we page fault, as reading esp 
     out of the struct intr_frame passed to page_fault() would yield an undefined value,
     not the user stack pointer. */
  thread_current ()->esp = f->esp;

  /* Sorry about the switch-case, but a hashtable is slower 
     and this is more readable. */ 
  switch (intr) {
    case SYS_HALT:
      halt ();
      break;
    case SYS_EXIT:
      exit ((int) first_arg(f));
      break;
    case SYS_EXEC:
      f->eax = exec ((const char *) first_arg(f));
      break;
    case SYS_WAIT:
      f->eax = wait ((pid_t) first_arg(f));
      break;
    case SYS_CREATE:
      f->eax = create ((const char *) first_arg(f), (unsigned) second_arg(f));
      break;
    case SYS_REMOVE:
      f->eax = remove ((const char *) first_arg(f));
      break;
    case SYS_OPEN:
      f->eax = open ((const char *) first_arg(f));
      break;
    case SYS_FILESIZE:
      f->eax = filesize ((int) first_arg(f));
      break;
    case SYS_READ:
      f->eax = read ((int) first_arg(f), second_arg(f), (unsigned int) third_arg(f)); 
      break;
    case SYS_WRITE:
      f->eax = write ((int) first_arg(f), second_arg(f), (unsigned int) third_arg(f));
      break;
    case SYS_SEEK:
      seek ((int) first_arg(f), (unsigned int) second_arg(f));
      break;
    case SYS_TELL:
      f->eax = tell ((int) first_arg(f));
      break;
    case SYS_CLOSE:
      close ((int) second_arg(f));
      break;
    case SYS_MMAP:
      f->eax = mmap ((int) first_arg(f), second_arg(f));
      break;
    case SYS_MUNMAP:
      munmap ((mapid_t) first_arg(f));
      break;
    default:
      /* Will terminate the current user process if an 
        invalid system call is used */
      exit (-1);
  }
}

/* Returns first system call argument from the stack */
static void *first_arg (struct intr_frame *f) {
  return (void *) get_int ((uint8_t *) f->esp + 4);
}

/* Returns second system call argument from the stack */
static void *second_arg (struct intr_frame *f) {
  return (void *) get_int ((uint8_t *) f->esp + 8);
}

/* Returns third system call argument from the stack */
static void *third_arg (struct intr_frame *f) {
  return (void *) get_int ((uint8_t *) f->esp + 12);
}

