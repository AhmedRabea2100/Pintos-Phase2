#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "devices/shutdown.h" // for SYS_HALT
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/kernel/list.h"


void
syscall_init (void)
{
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    lock_init(&write_lock);
}

int
take(struct intr_frame *f,int num){
    return (*((int *) f->esp + num));
}



static void
syscall_handler (struct intr_frame *f) {
    if(!pointer_validation(f)) {
        exit(-1);
    }
    int temp = *(int*)f->esp;
    switch(temp) {
        
        case SYS_EXEC:{
            wrapper_exec(f);
            break;
        }
        case SYS_WAIT:{
            wrapper_wait(f);
            break;
        }
        case SYS_HALT: {
            printf("(halt) begin\n");
            shutdown_power_off();
            break;
        }
        case SYS_EXIT: {
            wrapper_exit(f);
            break;
        }
        case SYS_CREATE:{
            wrapper_create(f);
            break;
        }case SYS_OPEN :{
            wrapper_open(f);
            break;
        }case SYS_CLOSE:{
            wrapper_close(f);
            break;
        }
        case SYS_WRITE: {
            wrapper_write(f);
            break;
        }
        case SYS_READ :{
            wrapper_read(f);
            break;
        }case SYS_FILESIZE:{

            get_size(f);
            break;
        }case SYS_REMOVE:{
            wrapper_remove(f);
            break;
        }
        case SYS_SEEK:{
            seek(f);
            break;
        }case SYS_TELL:{
            tell(f);
            break;
        }
        default:{
            // negative area
        }
    }

}

void wrapper_exit(struct intr_frame *f){
    int s =  take(f,1);
    if(!is_user_vaddr(s)) {
        f->eax = -1;
        exit(-1);
    }
    f->eax = s;
    exit(s);
}

void wrapper_exec(struct intr_frame *f){
    f->eax = process_execute((char *)  take(f,1));
}

void wrapper_wait(struct intr_frame *f){
    if(!validation((int*)f->esp + 1))
        exit(-1);
    //tid_t tid =  take(f,1);
    f->eax =  process_wait( (tid_t) take(f,1));
}
void wrapper_create(struct intr_frame *f){
    char * name = (char * ) take(f,1);
    int size = (unsigned)  take(f,2);
    int temp = 0;
    if(!validation (name)){
        exit(-1);
    }
    lock_acquire(&write_lock);
    temp = filesys_create (name,size);
    lock_release(&write_lock);
    f->eax = temp;
}

void wrapper_open(struct intr_frame *f){
    char * file_name = (char *)  take(f,1);
    static unsigned long curr_fd = 2;
    if(!validation (file_name)){
        exit(-1);
    }
    lock_acquire(&write_lock);
    struct file * file  = filesys_open(file_name);
    lock_release(&write_lock);
    if(file!=NULL){
        struct user_file* user_file = (struct user_file*) malloc(sizeof(struct user_file));
        int file_fd = curr_fd;
        user_file->fd = curr_fd;
        user_file->file = file;
        lock_acquire(&write_lock);
        curr_fd++;
        lock_release(&write_lock);
        struct list_elem *elem = &user_file->elem;
        list_push_back(&thread_current()->files_used, elem);
        f->eax =file_fd;
    }else{
        f->eax = -1;
    }
}

void wrapper_close(struct intr_frame *f){
    int fd = (int)  take(f,1);
    struct user_file  *file = get_file(fd);
    if(fd<2){
        exit(-1);
    }
    if(file==NULL){
        f->eax = -1;
    } else {
        lock_acquire(&write_lock);
        file_close(file->file);
        lock_release(&write_lock);
        list_remove(&file->elem);
        f->eax = 1;
    }
}

void get_size(struct intr_frame *f){
    int fd = take(f,1);
    struct user_file * file = get_file(fd);
    if(file !=  NULL){

        lock_acquire(&write_lock);
        f->eax = file_length(file->file);
        lock_release(&write_lock);
    }else{
        f->eax = -1;
    }
}

void seek(struct intr_frame *f){
    int fd = (int )  take(f,1);
    unsigned position = (unsigned) take(f,2);
    struct user_file * file = get_file(fd);
    if(file !=  NULL){
        lock_acquire(&write_lock);
        file_seek(file->file,position);
        f->eax = position;
        lock_release(&write_lock);
    }else{
        f->eax =- 1;
    }
}
void tell(struct intr_frame *f){
    int fd = (int )  take(f,1);
    struct user_file * file = get_file(fd);
    if(file !=  NULL){
        lock_acquire(&write_lock);
        f->eax = file_tell(file->file);
        lock_release(&write_lock);
    }else{
        f->eax =- 1;
    }
}

void wrapper_remove(struct intr_frame *f){
    char *  file_name = (char *) take(f,1);
    int temp = -1;
    bool valid = validation (file_name);
    if(!valid){
        exit(-1);
    }
    lock_acquire(&write_lock);
    temp = filesys_remove(file_name);
    lock_release(&write_lock);
    f->eax = temp;
}

int write(struct intr_frame *f){
    int fd = (int )  take(f,1);
    char * buffer = (char * )  take(f,2);
    unsigned size = *((unsigned *) f->esp + 3);
    struct user_file *file = get_file(fd);
    if(fd ==1){
        lock_acquire(&write_lock);
        putbuf(buffer, size);
        lock_release(&write_lock);
        return size;
    }

    if(file != NULL){
        int result = 0;
        lock_acquire(&write_lock);
        result= file_write(file->file,buffer,size);
        lock_release(&write_lock);
        return result;

    }else{
        return -1;
    }
}
void wrapper_read(struct intr_frame *f){
    int fd = (int )  take(f,1);
    char * buffer = (char * )  take(f,2);
    bool temp = fd == 1 ;
    bool valid = validation(buffer);
    if(temp || !valid){
        exit(-1);
    }
    f->eax = read(f);
}


void wrapper_write(struct intr_frame *f){
    int fd = (int )  take(f,1);
    char * buffer = (char * )  take(f,2);
    bool temp = fd == 0;
    bool valid = validation(buffer);
    if(temp || !valid){
        exit(-1);
    }
    f->eax = write(f);
}
int read(struct intr_frame *f){
    int fd = (int )  take(f,1);
    char * buffer = (char * )  take(f,2);
    unsigned size = *((unsigned *) f->esp + 3);

    if(fd ==0){
        //while (size--)
        for(int i=0;i<size;i++)
        {
            lock_acquire(&write_lock);
            char c = input_getc();
            lock_release(&write_lock);
            buffer+=c;
        }
        return size;
    }

    struct user_file * user_file =get_file(fd);
    if(user_file==NULL){
        return -1;
    }else{
        struct file * file = user_file->file;
        lock_acquire(&write_lock);
        size = file_read(file,buffer,size);
        lock_release(&write_lock);
        return size;
    }
}



// check stack pointer validation
bool pointer_validation(struct intr_frame *f ){
    bool temp = validation((int*)f->esp);
    bool temp2 = ((*(int*)f->esp) < 0);
    bool temp3 = (*(int*)f->esp) > 12;
    return  temp || temp2  || temp3;
}


struct user_file *  get_file( int  fd){
    struct list *l  = &(thread_current()->files_used);
    struct list_elem* e = list_begin(l);
    while (e!=list_end(l)){
        struct user_file*  file = list_entry(e, struct user_file , elem);
        if((file->fd) ==fd){
            return file;
        }
        e =list_next(e);
    }

    return NULL;
}


bool validation( void * name){
    return name!=NULL && is_user_vaddr(name)
    && pagedir_get_page(thread_current()->pagedir, name) != NULL;
}

// exit process
void exit(int status){
    char * ptr;
    char * name = thread_current()->name;
    char * exe = strtok_r(name, " ", &ptr);
    thread_current()->exitFlag = status;
    printf("%s: exit(%d)\n",exe,status);
    thread_exit();
}
