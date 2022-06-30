#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "stdbool.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

void syscall_init (void);
static void syscall_handler (struct intr_frame *f);
void exit(int status);
void tell(struct intr_frame *f);
void seek(struct intr_frame *f);
void get_size(struct intr_frame *f);
int read(struct intr_frame *f);
bool validation(void * name);
bool pointer_validation(struct intr_frame *f);
struct lock write_lock;
int write(struct intr_frame *f);
struct user_file *  get_file( int  fd);

#endif /* userprog/syscall.h */

