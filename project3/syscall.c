#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <user/syscall.h>
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include <list.h>
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

#include "vm/page.h"
#include "vm/frame.h"

static void syscall_handler (struct intr_frame *);

/* System calls */
static void syscall_halt(void);
static int syscall_wait(pid_t pid);
static pid_t syscall_exec(const char* cmd_line);
static bool syscall_create(const char* file, unsigned initial_size);
static bool syscall_remove(const char* file);
static int syscall_open(const char* file);
static int syscall_filesize(int fd);
static int syscall_read(int fd, void* buffer, unsigned size);
static int syscall_write (int fd, const void *buffer, unsigned size);
static void syscall_seek(int fd, unsigned position);
static unsigned syscall_tell(int fd);
static void syscall_close(int fd);
/* Utilities */
static struct file* fd2file ( int fd );   /* Convert fd to file pointer.  */
static void ffdlist_remove( int fd );        /* Remove from file_fd_list. */
static int ffdlist_push( struct file* file );/* Insert into file_fd_list. */
static bool is_valid_uservaddr( void* ptr);/* Check the pointer is valid. */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&filelock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* interrupt frame's argument stack */
  CURR_ESP = f->esp;
  if (!is_valid_uservaddr(f->esp)){
    syscall_exit(-1);
  }
  /* Update the current stack pointer */
  CURR_ESP = f->esp;

  /* Allocate system call code and arguments */
  int syscall_code = *(int *)(f->esp);
  void* arg1 = (int *)f->esp + 1;
  void* arg2 = (int *)f->esp + 2;
  void* arg3 = (int *)f->esp + 3;

  /* syscall_functions' arguments */
  int status;
  int fd;
  const void *buffer_write;
  void *buffer_read;
  unsigned size, initial_size, position;
  pid_t pid;
  const char* file;
  const char* cmd_line;

  /* main handler */
  switch(syscall_code){
    case SYS_EXEC:
      if( arg1 == NULL || !is_valid_uservaddr(arg1) )
        syscall_exit(-1);
      cmd_line = *(char **) arg1;
      if( cmd_line == NULL || !is_valid_uservaddr(cmd_line) )
        syscall_exit(-1);
      f->eax = syscall_exec(cmd_line);
      break;

    case SYS_HALT:
      syscall_halt();
      break;

    case SYS_EXIT:
      if (!is_valid_uservaddr(arg1))
        syscall_exit(-1);
      status = *(int *)arg1;
      syscall_exit(status);
      break;
    
    case SYS_WAIT:
      if (!is_valid_uservaddr(arg1))
        syscall_exit(-1);
      pid = *(pid_t *) arg1;
      f->eax = syscall_wait(pid);
      break;
    
    case SYS_CREATE:
      if(arg1 == NULL || !is_valid_uservaddr(arg1) || !is_valid_uservaddr(arg2))
        syscall_exit(-1);
      file = *(char **) arg1;
      if(file == NULL || !is_valid_uservaddr(file))
        syscall_exit(-1);
      initial_size = *(unsigned *) arg2;
      f->eax = syscall_create(file, initial_size);
      break;

    case SYS_REMOVE:
      if(arg1 == NULL || !is_valid_uservaddr(arg1))
        syscall_exit(-1);
      file = *(char **) arg1;
      if(file == NULL || !is_valid_uservaddr(file))
        syscall_exit(-1);
      f->eax = syscall_remove(file);
      break;
  
    case SYS_OPEN:
      if(arg1 == NULL || !is_valid_uservaddr(arg1))
        syscall_exit(-1);
      file = *(char **) arg1;
      if(file == NULL || !is_valid_uservaddr(file))
        syscall_exit(-1);
      f->eax = syscall_open(file);
      break;

    case SYS_FILESIZE:
      if(arg1 == NULL || !is_valid_uservaddr(arg1))
        syscall_exit(-1);
      fd = *(int *) arg1;
      f->eax = syscall_filesize(fd);
      break;

    case SYS_READ:
      if(arg1 == NULL || !is_valid_uservaddr(arg1) 
          || arg2 == NULL || !is_valid_uservaddr(arg2)
          || arg3 == NULL || !is_valid_uservaddr(arg3))
        syscall_exit(-1);
      fd = *(int *) arg1;
      buffer_read = *(void **) arg2;
      size = *(unsigned *) arg3;
      f->eax = syscall_read( fd, buffer_read, size );
      break;

    case SYS_WRITE:
      if (!is_valid_uservaddr(arg3))
        syscall_exit(-1);
      fd = *(int *) arg1;
      buffer_write = *(char **) arg2;
      size = *(unsigned *) arg3;
      f->eax = syscall_write(fd, buffer_write, size);
      break;

    case SYS_SEEK:
      if(arg1 == NULL || !is_valid_uservaddr(arg1)
          || arg2 == NULL || !is_valid_uservaddr(arg2))
        syscall_exit(-1);
      fd = *(int *) arg1;
      position = *(unsigned *) arg2;
      syscall_seek(fd, position);
      break;

    case SYS_TELL:
      if(arg1 == NULL || !is_valid_uservaddr(arg1))
        syscall_exit(-1);
      fd = *(int *) arg1;
      f->eax = syscall_tell(fd);
      break;

    case SYS_CLOSE:
      if(arg1 == NULL || !is_valid_uservaddr(arg1))
        syscall_exit(-1);
      fd = *(int *) arg1;
      syscall_close(fd);
      break;
  }
}

//#################################################################
//#################################################################
//    System call functions
//#################################################################
//#################################################################
static void
syscall_halt(void){
  power_off();
}

static pid_t syscall_exec( const char* cmd_line ){
  pid_t pid;

  pid = process_execute(cmd_line);
  return pid;
}

void
syscall_exit ( int status ){
  struct thread *curr;
  const char *name;
  
  curr = thread_current();
  name = thread_name();

  printf("%s: exit(%d)\n", name, status);
  
  curr->exit_stat_code = status; 

  thread_exit();
}

static int
syscall_wait(pid_t pid){
  return process_wait(pid);
}

static bool
syscall_create( const char *file, unsigned initial_size ){
  bool success;
  
  filelock_acquire();
  success = filesys_create( file, initial_size );
  filelock_release();

  return success;
}

static bool
syscall_remove( const char *file ){
  bool success;

  filelock_acquire();
  success = filesys_remove( file );
  filelock_release();

  return success;
}

static int
syscall_open( const char* file ){
  int fd;
  struct file* f;

  filelock_acquire();
  f = filesys_open(file);
  filelock_release();

  if( f == NULL ){
    return -1;
  }
  else{
    filelock_acquire();
    fd = ffdlist_push(f);
    filelock_release();
  }
  return fd;
}

static int
syscall_filesize( int fd ){
  struct file* file;
  int filesize;

  file = fd2file(fd);
  if( file == NULL ){
    syscall_exit(-1);
  }

  filelock_acquire();
  filesize = file_length(file);
  filelock_release();

  return filesize;
}

static int
syscall_read( int fd, void* buffer, unsigned size ){
  struct file* file;
  int bytes;
  unsigned i;
  uint8_t tmp;

  if( fd < 0 || fd == 1 || !is_valid_uservaddr( buffer ) ){
    syscall_exit(-1);
  }

  if( fd == 0 ){
    for( i == 0 ; i < size ; i++ ){
      tmp = input_getc();
      *((uint8_t *)buffer + i) = tmp;
      if( tmp == 0 )
        break;
    }
    return i;
  }
  else{
    file = fd2file(fd);
    if( file == NULL ){
      syscall_exit(-1);
    }
    filelock_acquire();
    bytes = file_read(file, buffer, size);
    filelock_release();
    return bytes;
  }
}

static int
syscall_write ( int fd, const void *buffer, unsigned size ){
  struct file* file;
  int bytes;

  if( fd <= 0 || !is_valid_uservaddr( buffer ) ){ // no input access, and have to valify
    syscall_exit(-1);
  }

  if( fd == 1 ){ // output access
    putbuf(buffer, size);
    return size;
  }
  else{
    file = fd2file(fd);
    if( file == NULL ){ // no such file curr thread has
      syscall_exit(-1);
    }
    filelock_acquire();
    bytes = file_write(file, buffer, size);
    filelock_release();
    return bytes;
  }
}

static void
syscall_seek( int fd, unsigned position){
  struct file* file;

  file = fd2file(fd);
  if( file == NULL )
    syscall_exit(-1);

  filelock_acquire();
  file_seek(file, position);
  filelock_release();
}

static unsigned
syscall_tell( int fd ){
  struct file* file;
  unsigned offset;

  file = fd2file(fd);
  if( file == NULL )
    syscall_exit(-1);

  filelock_acquire();
  offset = file_tell(file);
  filelock_release();
  return offset;
}

static void
syscall_close( int fd ){
  struct file* file;
  
  file = fd2file(fd);
  if( file == NULL )
    syscall_exit(-1);

  filelock_acquire();
  file_close(file);
  ffdlist_remove(fd);
  filelock_release();
}

//###########################################################
//###########################################################
//    Utilities for manipulating filelock, file_fd_list, 
//    file_fd_pair, etc.
//###########################################################
//###########################################################
static void
ffdlist_remove( int fd ){
  struct thread* curr;
  struct file_fd_pair* ffd_pair;
  struct list_elem* e;

  curr = thread_current();
  for( e = list_begin(&curr->file_fd_list);
      e != list_end(&curr->file_fd_list);
      e = list_next(e)){
    ffd_pair = list_entry(e, struct file_fd_pair, elem);
    if( ffd_pair->fd == fd ){
      list_remove(e);
      free(ffd_pair);
      break; // there is the only one file with fd
    }
  }
}

static int
ffdlist_push( struct file* file ){
  struct thread* curr;
  struct file_fd_pair* ffd_pair;

  curr = thread_current();
  curr->fd_max++;
  ffd_pair = (struct file_fd_pair*) malloc(sizeof(struct file_fd_pair));
  ffd_pair->file = file;
  ffd_pair->fd = curr->fd_max;

  list_push_back(&curr->file_fd_list, &ffd_pair->elem);

  return curr->fd_max;
}

static struct file*
fd2file( int fd ){
  
  if( fd < 2 )
    return NULL;
  
  struct thread* curr = thread_current();
  struct file_fd_pair* ffd_pair;
  struct list_elem* e;

  for ( e = list_begin(&curr->file_fd_list);
      e != list_end(&curr->file_fd_list);
      e = list_next(e))
  {
    ffd_pair = list_entry(e, struct file_fd_pair, elem);
    if ( ffd_pair->fd == fd){
      return ffd_pair->file;
    }
    else{
      continue;
    }
  }
  return NULL;
}

static bool 
is_valid_uservaddr(void* ptr){
  return is_user_vaddr( ptr );
}

void 
filelock_acquire(void){
  lock_acquire(&filelock);
}
void 
filelock_release(void){
  lock_release(&filelock);
}

void*
get_curr_esp(){
  return CURR_ESP;
}
