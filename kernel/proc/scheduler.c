#include "scheduler.h"
#include "kernel/console.h"
#include "arch/x64/gdt.h"
#include <string.h>

extern process_t *proc_list;
extern process_t *proc_cur;

void sched_init(void)
{
    klog(LOG_OK, "scheduler", "Preemptive scheduler initialised");
}

void sched_admit(process_t *proc)
{
    (void)proc;
}

void sched_remove(process_t *proc)
{
    (void)proc;
}

void sched_yield(void)
{
    __asm__ volatile("int $32");
}

void sched_wakeup(process_t *proc)
{
    if (!proc) return;
    proc->state = PROC_STATE_RUNNING;
}

cpu_state_t *sched_handler(cpu_state_t *state)
{
    process_t *current = proc_current();

    if (state->int_no != 32) return state;

    process_t *next = (current && current->next) ? current->next : proc_list;

    while (next && next->state != PROC_STATE_RUNNING) {
        next = next->next ? next->next : proc_list;
        if (next == current) break;
    }

    if (!next || next == current || next->state != PROC_STATE_RUNNING) {
        return state;
    }

    if (current) {
        current->kernel_rsp = (uint64_t)state;
    }

    proc_cur = next;
    tss_set_kernel_stack(next->kstack_top);
    vmm_switch_address_space(next->as);

    return (cpu_state_t *)next->kernel_rsp;
}

void sched_schedule(cpu_state_t *state)
{
    (void)state;
}
