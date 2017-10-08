#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock filelock;                 /* lock for file system */
void syscall_init (void);
void syscall_exit (int status);
void filelock_acquire(void);
void filelock_release(void);


#endif /* userprog/syscall.h */
