/* kernel/proc/scheduler.h
 * Zirvium Kernel — Preemptive Scheduler
 */
#ifndef ZIRVIUM_KERNEL_PROC_SCHEDULER_H
#define ZIRVIUM_KERNEL_PROC_SCHEDULER_H

#include "process.h"
#include "arch/x64/idt.h"

/**
 * sched_init - Initialize the scheduler.
 */
void sched_init(void);

/**
 * sched_yield - Voluntarily yield the CPU to another process.
 */
void sched_yield(void);

/**
 * sched_handler - The core scheduling logic. Called from the timer interrupt.
 * @state: Current CPU state to be saved if a context switch occurs.
 * Returns the new CPU state (stack pointer).
 */
cpu_state_t *sched_handler(cpu_state_t *state);

/**
 * sched_admit - Add a process to the scheduler's ready queue.
 */
void sched_admit(process_t *proc);

/**
 * sched_remove - Remove a process from the scheduler.
 */
void sched_remove(process_t *proc);

#endif /* ZIRVIUM_KERNEL_PROC_SCHEDULER_H */
