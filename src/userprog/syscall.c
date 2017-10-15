#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#include "devices/shutdown.h"
#include "filesys/off_t.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //printf ("system call!\n");
  //thread_exit ();

  // system call handling
  switch(*(int*)(f->esp)){
    case SYS_HALT:	shutdown_power_off();	break;
    case SYS_EXIT:	syscall_exit(thread_current(), *((int*)(f->esp)+1));	break;
    case SYS_EXEC:	break;//syscall_exec();	break; 
    case SYS_WAIT:	break;//syscall_wait();	break;
    case SYS_CREATE:break;//syscall_create();	break;
    case SYS_REMOVE:break;//syscall_remove();	break;
    case SYS_OPEN:	break;//syscall_open();		break;
    case SYS_FILESIZE:	break;//syscall_filesize();	break;
    case SYS_READ: 		break;//syscall_read();		break;
    case SYS_WRITE: 	syscall_write(*((int*)(f->esp)+1)
    								  *((char**)(f->esp)+2)
    								  *((off_t*)(f->esp)+3));	break;
    case SYS_SEEK:		break;//syscall_seek();		break;
    case SYS_TELL: 		break;//syscall_tell();		break;
    case SYS_CLOSE: 	break;//syscall_close();	break;
    default: break;
  }
}


void 
syscall_exit(struct thread *t, int status){ 
	printf (“%s: exit(%d)\n”, t->name, status);
	thread_exit();
}

off_t
syscall_write(int fd, char* buffer, off_t size){
	if(fd==1){
		putbuf(pagedir_get_page,size);
	}

}