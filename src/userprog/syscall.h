#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include "filesys/off_t.h"

void syscall_init (void);

void syscall_exit(struct thread *t, int status);

off_t syscall_write(int fd, char* buffer, off_t size);

#endif /* userprog/syscall.h */
