#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/user/syscall.h"
#include "threads/thread.h"
#include "filesys/off_t.h"

void syscall_init (void);

pid_t syscall_exec(char* cmd_str);
void syscall_exit(int status);
int syscall_wait(pid_t pid)

off_t syscall_write(int fd, char* buffer, off_t size);

#endif /* userprog/syscall.h */
