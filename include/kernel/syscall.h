#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <kernel/types.h>

#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3
#define SYS_FORK    4
#define SYS_EXEC    5
#define SYS_EXIT    6
#define SYS_GETPID  7
#define SYS_KILL    8
#define SYS_ADMIN   9

/* DisplayJet syscalls */
#define SYS_DISPLAYJET_CREATE_SESSION   10
#define SYS_DISPLAYJET_ALLOC_BUFFER     11
#define SYS_DISPLAYJET_FREE_BUFFER      12
#define SYS_DISPLAYJET_UPDATE_BUFFER    13
#define SYS_DISPLAYJET_REQUEST_ACCESS   14
#define SYS_DISPLAYJET_VERIFY_KEY       15
#define SYS_DISPLAYJET_DECRYPT_ONCE     16

void syscall_init(void);
int64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, 
                        uint64_t arg3, uint64_t arg4, uint64_t arg5);

#endif
