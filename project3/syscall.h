#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock filelock;                 /* lock for file system */
void syscall_init (void);
void syscall_exit (int status);
void filelock_acquire(void);
void filelock_release(void);
void* get_curr_esp(void);             /* Getter for CURR_ESP */

void* CURR_ESP;                       /* Stack pointer of current thread. */
                                      /* Used in exception.c to kernel stack growth in system call. */

#endif /* userprog/syscall.h */
