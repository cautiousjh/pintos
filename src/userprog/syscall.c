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
#include "devices/shutdown.h"
#include "vm/frame.h"
#include "vm/page.h"

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
    	syscall_close(*((char**)(f->esp)+1)); 
     	break;
    case SYS_MMAP:
    	f->eax = syscall_mmap(*((int*)(f->esp)+1),*((unsigned*)(f->esp)+2));
    	break;
    case SYS_MUNMAP:
    	syscall_munmap(*((mapid_t*)(f->esp)+1));
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
	return filesys_create(name, size);
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

	ASSERT_EXIT(name);
	
	// filesys open
	open_file = filesys_open(name);
	if(!open_file)
		return -1;

	//add file to thread
	felem = malloc(sizeof(struct file_elem));
	felem->fd = set_new_fd();
	felem->this_file = open_file;
	felem->mmapid = -1;
	felem->addr = NULL;
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


mapid_t 
syscall_mmap (int fd, void *addr)
{
	struct file_elem* felem;
	struct thread* curr_thread = thread_current();
	size_t ofs;

	// fd, addr0, addr aligned validation
	if(fd<=1 || addr == NULL || (uint32_t)addr%PGSIZE)
		return -1;

	lock_acquire(&file_lock);
	
	// file validation
	felem = get_file_elem(fd);
	if(felem->this_file == NULL){
		lock_release(&file_lock);
		return -1;
	}

	//file size validation
	size_t file_size = file_length(felem->this_file);
	if(file_size==0){
		lock_release(&file_lock);
		return -1;
	}

	// page duplicated validation
	for(ofs = 0; ofs < file_size; ofs+=PGSIZE){
		if(page_table_lookup(addr+ofs)){
			lock_release(&file_lock);
			return -1;
		}
	}

	// page mapping
	for(ofs = 0; ofs < file_size; ofs+=PGSIZE){
		size_t read_bytes = ofs+PGSIZE < file_size? PGSIZE : file_size-ofs;

		struct page* new_page = (struct page*)malloc(sizeof(struct page));
		new_page->addr = addr + ofs;
		new_page->frame_entry = NULL;
		new_page->writable = true; 
		new_page->status = IN_FILESYS;
		new_page->in_stack_page = false;
		new_page->file_ptr = felem->this_file;
		new_page->offset = ofs;
		new_page->read_bytes = read_bytes;
		add_page(curr_thread, new_page);
	}

	// mmapid mapping
	curr_thread->mmapid_cnt++;
	felem->mmapid = curr_thread->mmapid_cnt;
	felem->addr = addr;

	lock_release(&file_lock);

	return felem->mmapid;
}

void 
syscall_munmap (mapid_t mmapid)
{
	struct file_elem* felem = NULL;
	struct list_elem* iter;
	struct page* unmap_page;
	size_t ofs, file_size;
	struct thread* curr_thread = thread_current();

	// mmapid validation
	if(mmapid > curr_thread->mmapid_cnt)
		return;

	lock_acquire(&file_lock);

	// find file_elem
	for(iter = list_begin(&curr_thread->fd_list);
		iter!= list_end(&curr_thread->fd_list);
		iter = iter->next){
		felem = list_entry(iter, struct file_elem, elem);
		if(felem->mmapid == mmapid)
			break;
	}

	// iterate each page
	if(iter != list_end(&curr_thread->fd_list)){
		file_size = file_length(felem->this_file);
		for(ofs=0; ofs<file_size; ofs+=PGSIZE){
			size_t bytes = ofs+PGSIZE < file_size? PGSIZE : file_size-ofs;
			unmap_page = page_table_lookup(felem->addr+ofs);
			if(unmap_page == NULL)
				continue;

			if(unmap_page->status == IN_FRAME_TABLE){
				if(pagedir_is_dirty(curr_thread->pagedir,unmap_page->addr))
					file_write_at(felem->this_file, unmap_page->addr, bytes, ofs);
				free_frame(unmap_page->frame_entry);
				free(unmap_page->frame_entry);
				pagedir_clear_page(curr_thread->pagedir, unmap_page->addr);
			}
			else if(unmap_page->status == IN_SWAP_DISK){
				return;
			}
		}
	}

	lock_release(&file_lock);

}