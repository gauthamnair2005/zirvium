#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <kernel/types.h>

#define PROC_STATE_RUNNING  0
#define PROC_STATE_READY    1
#define PROC_STATE_BLOCKED  2
#define PROC_STATE_ZOMBIE   3

#define PROC_PERM_ROOT      0x1

typedef struct process {
    pid_t pid;
    uint64_t state;
    uint32_t permissions;
    uint64_t *page_table;
    
    uint64_t rsp;
    uint64_t rip;
    uint64_t stack_base;
    
    struct process *next;
    struct process *parent;
} process_t;

void proc_init(void);
process_t *proc_create(void (*entry)(void));
void proc_exit(int status);
void proc_schedule(void);
process_t *proc_current(void);

#endif
