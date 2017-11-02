#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"

#define ASSERT_EXIT( COND ) { if(!(COND)) syscall_exit(-1); }

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
    	break;
    case SYS_WAIT:	
    	f->eax = syscall_wait(*((int*)(f->esp)+1));
    	break;
    case SYS_CREATE:
    	f->eax = syscall_create(*((char**)(f->esp)+1), *((unsigned*)(f->esp)+2));
    	break;
    case SYS_REMOVE:
    	f->eax = syscall_remove(*((char**)(f->esp)+1));
    	break;
    case SYS_OPEN:
    	f->eax = syscall_open(*((char**)(f->esp)+1));
    	break;
    case SYS_FILESIZE:
    	f->eax = syscall_filesize(*((char**)(f->esp)+1));
    	break;
    case SYS_READ:
    	f->eax = syscall_read(*((int*)(f->esp)+1),
    						  *((char**)(f->esp)+2),
    				  		  *((off_t*)(f->esp)+3)); 
    	break;
    case SYS_WRITE: 
    	f->eax = syscall_write(*((int*)(f->esp)+1),
    						   *((char**)(f->esp)+2),
    				  		   *((off_t*)(f->esp)+3));
    	break;
    case SYS_SEEK:
    	syscall_seek(*((int*)(f->esp)+1), *((unsigned*)(f->esp)+2)); 
    	break;
    case SYS_TELL:
    	f->eax = syscall_tell(*((char**)(f->esp)+1)); 
     	break;
    case SYS_CLOSE:
    	syscall_close(*((char**)(f->esp)+1)); 
     	break;
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

bool
syscall_create(const char* name, unsigned size)
{
	ASSERT_EXIT(name);
	return filesys_create(name, size);
}

bool 
syscall_remove(const char* name)
{
	return -1;
}

int
syscall_open(const char* name)
{
	return -1;

}

int
syscall_filesize(int fd)
{
	return -1;

}

int
syscall_read(int fd, char* buffer, off_t size)
{
	return -1;

}

off_t
syscall_write(int fd, char* buffer, off_t size)
{
	if(fd==1){
		putbuf(buffer,size);
	}

}
void
syscall_seek(int fd, unsigned position)
{
	return -1;

}

unsigned
syscall_tell(int fd)
{
	return -1;

}

void
syscall_close(int fd)
{
	return -1;

}


