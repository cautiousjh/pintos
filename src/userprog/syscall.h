#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/user/syscall.h"
#include "threads/thread.h"
#include "filesys/off_t.h"

void syscall_init (void);

pid_t syscall_exec(char* cmd_str);
void syscall_exit(int status);
int syscall_wait(pid_t pid);
bool syscall_create(const char* name, unsigned size);
bool syscall_remove(const char* name);
int syscall_open(const char* name);
int syscall_filesize(int fd);
int syscall_read(int fd, char* buffer, off_t size);
off_t syscall_write(int fd, char* buffer, off_t size);
void syscall_seek(int fd, unsigned position);
unsigned syscall_tell(int fd);
void syscall_close(int fd);

bool syscall_chdir(const char* dir);
bool syscall_mkdir(const char* dir);
bool syscall_readdir(int fd, char *name);
bool syscall_isdir(int fd);
int syscall_inumber(int fd);


struct file_elem* get_file_elem(int fd);
bool close_file(int fd);
void close_all_file(struct list* fd_list);
int set_new_fd(void);

#endif /* userprog/syscall.h */
