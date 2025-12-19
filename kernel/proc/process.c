#include <kernel/process.h>
#include <kernel/memory.h>
#include <kernel/kernel.h>

static process_t *process_list = NULL;
static process_t *current_process = NULL;
static pid_t next_pid = 1;
static uint64_t process_count = 0;

void proc_init(void) {
    process_list = NULL;
    current_process = NULL;
    next_pid = 1;
    process_count = 0;
    
    kprintf("Process scheduler: Round-robin scheduling enabled\n");
}

process_t *proc_create(void (*entry)(void)) {
    process_t *proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) {
        kprintf("proc_create: Failed to allocate process structure\n");
        return NULL;
    }
    
    proc->pid = next_pid++;
    proc->state = PROC_STATE_READY;
    proc->rip = (uint64_t)entry;
    
    // Allocate 16KB stack
    void *stack = vmm_alloc(4);
    if (!stack) {
        kprintf("proc_create: Failed to allocate stack\n");
        kfree(proc);
        return NULL;
    }
    proc->rsp = (uint64_t)stack + (4 * PAGE_SIZE);
    
    proc->page_table = NULL;
    proc->parent = current_process;
    proc->next = process_list;
    process_list = proc;
    process_count++;
    
    return proc;
}

void proc_exit(int status) {
    (void)status;
    if (!current_process) return;
    
    current_process->state = PROC_STATE_ZOMBIE;
    if (process_count > 0) {
        process_count--;
    }
    
    proc_schedule();
}

void proc_schedule(void) {
    if (!process_list) return;
    
    process_t *next = process_list;
    while (next && next->state != PROC_STATE_READY) {
        next = next->next;
    }
    
    if (next) {
        current_process = next;
        current_process->state = PROC_STATE_RUNNING;
    }
}

process_t *proc_current(void) {
    return current_process;
}

uint64_t proc_get_count(void) {
    return process_count;
}
