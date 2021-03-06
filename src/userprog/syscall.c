#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "devices/shutdown.h"

#define ASSERT_EXIT( COND ) { if(!(COND)) syscall_exit(-1); }

static void syscall_handler (struct intr_frame *);

static struct lock file_lock;

void
syscall_init (void) 
{
  lock_init(&file_lock);
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
    	syscall_close(*((const char**)(f->esp)+1)); 
     	break;
     case SYS_CHDIR:
     	f->eax = syscall_chdir(*((char**)(f->esp)+1));
     	break;
     case SYS_MKDIR:
     	f->eax = syscall_mkdir(*((char**)(f->esp)+1));
     	break;
    case SYS_READDIR:
    	f->eax = syscall_readdir(*((char**)(f->esp)+1),*((int*)(f->esp)+2));
    	break;
    case SYS_ISDIR:
    	f->eax = syscall_isdir(*((int*)(f->esp)+1));
    	break;
    case SYS_INUMBER:
    	f->eax = syscall_inumber(*((int*)(f->esp)+1));
    	break;
    default: break;
  }
}

bool syscall_chdir(const char* path){
	struct thread *t = thread_current();
	struct dir *dir = dir_chdir(path);
	if(dir){
		dir_close(t->dir_current);
		t->dir_current = dir;
		return true;
	}
	else
		return false;
}
bool syscall_mkdir(const char* dir){
	return filesys_create(dir,0,true);	//TODO filesys create
}
bool syscall_readdir(int fd, char *name){
	ASSERT_EXIT(get_file_elem(fd)->this_file);
	struct file_elem* felem = get_file_elem(fd);
	if(!felem || felem->this_dir == NULL)
		return false;
	else 
		return dir_readdir(felem->this_dir, name);
}
bool syscall_isdir(int fd){
	ASSERT_EXIT(get_file_elem(fd)->this_file);
	return get_file_elem(fd)->this_dir != NULL;
}
int syscall_inumber(int fd){
	ASSERT_EXIT(get_file_elem(fd)->this_file);
	struct file_elem* felem = get_file_elem(fd);
	if(felem->this_dir)
		return inode_get_inumber(dir_get_inode(felem->this_dir));
	else
		return inode_get_inumber(file_get_inode(felem->this_file));
}


void 
syscall_exit(int status)
{
	struct thread * curr_thread = thread_current(); 
	struct list_elem* iter;

	// TODO: close files opened
	//close_all_file(&curr_thread->fd_list);

	// set exit code
	for(iter = list_begin(&curr_thread->parent->children);
	    iter != list_end(&curr_thread->parent->children);
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
	return filesys_create(name, size, false);
}

bool 
syscall_remove(const char* name)
{
	ASSERT_EXIT(name);
	return filesys_remove(name);
}

int
syscall_open(const char* name)
{
	struct file* open_file;
	struct file_elem* felem;
	struct inode* inode;
	struct dir* dir;
	struct dir* dir_current = thread_current()->dir_current;

	ASSERT_EXIT(name);
	
	// filesys open
	open_file = filesys_open(name);
	if(!open_file)
		return -1;

	//add file to thread
	felem = malloc(sizeof(struct file_elem));
	felem->fd = set_new_fd();
	felem->this_dir = NULL;
	felem->this_file = open_file;

	inode = file_get_inode(open_file);
	if(inode_get_parent(inode) != -1){	// if directory
		if(dir_current && 
			inode_get_inumber(dir_get_inode(dir_current)) == inode_get_inumber(inode)){
			dir_close(dir);
			dir = dir_reopen(dir_current);
		}
		else
			dir = dir_open(file_get_inode(open_file));
		felem->this_dir = dir;
	}
	list_push_back(&thread_current()->fd_list, &felem->elem);

	return felem->fd;
}

int
syscall_filesize(int fd)
{
	struct file* f;
	if(!(f = get_file_elem(fd)->this_file))
		return -1;
	return file_length(f);
}

int
syscall_read(int fd, char* buffer, off_t size)
{
	int i;
	ASSERT_EXIT(is_user_vaddr(buffer) && (buffer>0x08048000));
	ASSERT_EXIT(fd == STDIN_FILENO || get_file_elem(fd)->this_file);
	if(fd==STDIN_FILENO){
		for(i=0;i<size;i++)
			buffer[i] = input_getc();
		return i;
	}
	return file_read(get_file_elem(fd)->this_file, buffer, size);
}

off_t
syscall_write(int fd, char* buffer, off_t size)
{
	ASSERT_EXIT(is_user_vaddr(buffer) && (buffer>0x08048000));
	ASSERT_EXIT(fd == STDOUT_FILENO || get_file_elem(fd)->this_file);
	if(fd==STDOUT_FILENO){
		putbuf(buffer,size);
		return size;
	}
	return file_write(get_file_elem(fd)->this_file, buffer, size);
}
void
syscall_seek(int fd, unsigned position)
{
	ASSERT_EXIT(get_file_elem(fd)->this_file);
	return file_seek(get_file_elem(fd)->this_file, position);
}

unsigned
syscall_tell(int fd)
{
	ASSERT_EXIT(get_file_elem(fd)->this_file);
	return file_tell(get_file_elem(fd)->this_file);
}

void
syscall_close(int fd)
{
	ASSERT_EXIT(get_file_elem(fd)->this_file);
	if(fd == STDOUT_FILENO || fd == STDIN_FILENO)
		return;
	if(syscall_isdir(fd))
		dir_close(get_file_elem(fd)->this_dir);
	else
		ASSERT_EXIT(close_file(fd));
}


struct file_elem*
get_file_elem(int fd){
	struct thread* curr_thread = thread_current();
	struct list_elem* iter;
	
	if(list_empty(&curr_thread->fd_list))
		return NULL;

	for(iter = list_begin(&curr_thread->fd_list);
		iter != list_end(&curr_thread->fd_list);
		iter = iter->next)
		if(list_entry(iter,struct file_elem, elem)->fd == fd)
			return list_entry(iter,struct file_elem, elem);
	return NULL;
}

bool close_file(int fd)
{
	struct thread* curr_thread = thread_current();
	struct list_elem* iter;
	struct file_elem* felem;

	if(list_empty(&curr_thread->fd_list))
		return false;

	for(iter = list_begin(&curr_thread->fd_list);
		iter != list_end(&curr_thread->fd_list);
		iter = iter->next){
		felem = list_entry(iter, struct file_elem, elem);
		if(felem->fd == fd){
			// TODOOTODOTODOTODOTODOTOOTODOTOD
			if (felem->this_dir)
				dir_close(felem->this_dir);
			else
				file_close(felem->this_file);
			list_remove(iter);
			free(felem);
			return true;
		}
	}
	return false;
}

// NOT IMPLEMENTED
void close_all_file(struct list* fd_list)
{
	struct list_elem* iter;
	struct list_elm* next;
	struct file_elem* felem;

	if(list_empty(fd_list))
		return;

	for(iter = list_begin(fd_list);
		iter != list_end(fd_list);
		iter = next){
		next = iter->next;
		felem = list_entry(iter, struct file_elem, elem);
			// TODOOTODOTODOTODOTODOTOOTODOTOD
		if (felem->this_dir)
			dir_close(felem->this_dir);
		else
			file_close(felem->this_file);
		list_remove(iter);
		free(felem);
	}
	list_init(fd_list);
}


int
set_new_fd(void){
	static int global_fd = 2;
	int new_fd;

	lock_acquire(&file_lock);
	new_fd = global_fd++;
	lock_release(&file_lock);

	return new_fd;
}