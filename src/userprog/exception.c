#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/off_t.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "filesys/file.h"

extern struct lock filesystem_lock;

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill, "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill, "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill, "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  
         Shouldn't happen.  Panic the kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      PANIC ("Kernel bug - this shouldn't be possible!");
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to task 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();
 
  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

   if (user) {
      thread_current ()->esp = f->esp;
   }

   if (is_a_stack_access(fault_addr)) {
      grow_the_stack(fault_addr);
   }

  struct page *page = locate_page (fault_addr, thread_current()->page_table);

  if (page != NULL)
  {
    if (write && !page->writable)
      exit (-1);

    int old_level = intr_disable ();
    
    if (page->status == FRAME)
    {
      intr_set_level (old_level);
      return;
    } else if (page->status == SWAP)
    {
      struct swapslot *slot = (struct swapslot *) page->data;
      sema_down (&slot->sema);
      intr_set_level (old_level);
      if (page->data == NULL)
        return;
    } else
    {
      intr_set_level (old_level);
    }
    
    bool shared = false;
    struct frame *frame = alloc_frame (page, page->writable, page->node, &shared);
    
    if (!frame)
      PANIC ("failed to alloc frame");
    
    if (!pagedir_set_page (page->t->pagedir, page->addr, frame->kPage, page->writable))
      PANIC ("failed to set page");
    
    if (shared)
    {
      page->status = FRAME;
      return;
    }

    switch (page->status)
    {
      case FRAME:
        NOT_REACHED ();
      case SWAP:
        //Swap in
        get_from_swap (page, frame); 
        break;
      case FILE_SYS:
      {
        //Load from file
        struct file_data *fdata = (struct file_data *) page->data;
        bool pre_owned = filesystem_lock.holder == thread_current ();
        if (!pre_owned) {
          lock_acquire (&filesystem_lock);
        }
        file_seek (fdata->file, fdata->ofs);
        int bytes_read = file_read (fdata->file, frame->kPage, fdata->read_bytes);
        if (!pre_owned) {
          lock_release (&filesystem_lock);
        }
        if (bytes_read != fdata->read_bytes)
          PANIC ("FAILED TO READ SEGMENT!");
        memset (frame->kPage + fdata->read_bytes, 0, fdata->zero_bytes);
        page->node = file_get_inode (fdata->file);
        free (fdata);
        page->data = NULL;
        break;
      }
      case ZERO:
        //Zero page
        memset (page->addr, 0, PGSIZE);
        break;
    }

    page->status = FRAME; 
  } else {
    if (user)
    {
      exit (-1);
    } else
    {
      f->eip = (void *) f->eax; 
      f->eax = 0xffffffff;  
    }
  }
}

/* Determines whether fault_addr is a stack access or not. */
bool
is_a_stack_access (void *fault_addr) {
   return (fault_addr < PHYS_BASE && (fault_addr >= (thread_current ()->esp - 32)));
}

/* Grow the stack. */
void
grow_the_stack (void *fault_addr) {
   struct thread *current_thread = thread_current ();

   if ((uint32_t) (current_thread + PGSIZE) - (uint32_t) current_thread->esp >= MAX_STACK_SIZE) {
      exit(-1);
   }
      
   add_page(fault_addr, NULL, ZERO, current_thread->page_table, true);
}

