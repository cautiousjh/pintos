#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "devices/shutdown.h"

//#define ASSERT_EXIT( COND ) { if(!(COND))}

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // is esp valid?
  if(f->esp >= 0xc0000000 || f->esp <0x08048000 ||
  	 (f->esp >= 0xbffffffc && *(int*)(f->esp) != SYS_HALT) )
  	syscall_exit(-1);

  // system call handling
  switch(*(int*)(f->esp)){
    case SYS_HALT:	
    	shutdown_power_off();
    	break;
    case SYS_EXIT:	
    	syscall_exit(*((int*)(f->esp)+1));
    	break;
    case SYS_EXEC:	
    	f->eax = syscall_exec(*((char**)(f->esp)+1));
    	break;//	break; 
    case SYS_WAIT:	
    	f->eax = syscall_wait(*((int*)(f->esp)+1));
    	break;
    case SYS_CREATE:
    	//syscall_create(*((char**)(f->esp)+1), *((unsigned*)(f->esp)+2));
    	break;
    case SYS_REMOVE:
    	break;//syscall_remove();	break;
    case SYS_OPEN:
    	break;//syscall_open();		break;
    case SYS_FILESIZE:
    	break;//syscall_filesize();	break;
    case SYS_READ: 
    	break;//syscall_read();		break;
    case SYS_WRITE: 
    	syscall_write(*((int*)(f->esp)+1),
    				  *((char**)(f->esp)+2),
    				  *((off_t*)(f->esp)+3));
    	break;
    case SYS_SEEK:
    	break;//syscall_seek();		break;
    case SYS_TELL:
     	break;//syscall_tell();		break;
    case SYS_CLOSE:
     	break;//syscall_close();	break;
    default: break;
  }
}

void 
syscall_exit(int status)
{
	struct thread * curr_thread = thread_current(); 
	struct list_elem* iter;

	// TODO: close files opened

	// set exit code
	for(iter = list_begin(&curr_thread->children);
	    iter != list_end(&curr_thread->children);
	    iter = iter->next)
		if(list_entry(iter, struct child_thread, elem)->tid == curr_thread->tid){
	    	list_entry(iter, struct child_thread, elem)->exit_code = status;
	    	break;
		}
		
	printf ("%s: exit(%d)\n", curr_thread->name, status);
	thread_exit();
}

pid_t
syscall_exec(char* cmd_str)
{
	if(!pagedir_get_page(thread_current()->pagedir, cmd_str))
		return -1;
	else
		return process_execute(cmd_str);

}

int
syscall_wait(pid_t pid)
{
	return process_wait(pid);
}

//bool
//syscall_create(char* name, unsigned size)
//{

//}

off_t
syscall_write(int fd, char* buffer, off_t size)
{
	if(fd==1){
		putbuf(buffer,size);
	}

}