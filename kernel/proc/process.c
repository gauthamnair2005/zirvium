/* kernel/proc/process.c
 * Zirvium Kernel — Process management implementation
 */
#include "process.h"
#include "kernel/mm/vmm.h"
#include "kernel/mm/pmm.h"
#include "arch/x64/gdt.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── Kernel heap (kernel/mm/heap.c) ─────────────────────────────────────── */
extern void *kmalloc(size_t size, unsigned int flags);
extern void  kfree(void *ptr);

/* ── Process table ──────────────────────────────────────────────────────── */
static process_t *proc_list = NULL;   /* singly-linked list of all procs */
static process_t *proc_cur  = NULL;   /* currently running process        */
static uint32_t   next_pid  = 1;

/* ── proc_init ──────────────────────────────────────────────────────────── */
void proc_init(void)
{
    proc_list = NULL;
    proc_cur  = NULL;
    next_pid  = 1;
}

/* ── proc_create ─────────────────────────────────────────────────────────── */
process_t *proc_create(uint64_t entry_virt)
{
    process_t *proc = (process_t *)kmalloc(sizeof(process_t), 0);
    if (!proc) return NULL;
    memset(proc, 0, sizeof(process_t));

    proc->pid   = next_pid++;
    proc->state = PROC_STATE_RUNNING;

    /* Create a fresh user address space (upper half shared with kernel) */
    proc->as = vmm_create_address_space();
    if (!proc->as) {
        kfree(proc);
        return NULL;
    }

    /* Pre-map user stack pages (grows down from PROC_USTACK_TOP) */
    uint64_t stack_base = PROC_USTACK_TOP - (uint64_t)PROC_USTACK_PAGES * PAGE_SIZE;
    for (int i = 0; i < PROC_USTACK_PAGES; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) goto fail;
        vmm_map_page(proc->as,
                     stack_base + (uint64_t)i * PAGE_SIZE,
                     phys,
                     PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NO_EXEC);
    }

    proc->user_rip   = entry_virt;
    proc->user_rsp   = PROC_USTACK_TOP - 8; /* reserve space for null return addr */
    proc->brk        = PROC_HEAP_START;
    proc->mmap_cursor = PROC_MMAP_START;

    /* Allocate a kernel stack for this process (used by TSS on hardware IRQ) */
    uint64_t kstack_phys = pmm_alloc_pages(PROC_KSTACK_PAGES);
    if (!kstack_phys) goto fail;
    proc->kstack_top = (uint64_t)(uintptr_t)PHYS_TO_VIRT(kstack_phys)
                       + (uint64_t)PROC_KSTACK_PAGES * PAGE_SIZE;

    /* Link into process list */
    proc->next = proc_list;
    proc_list  = proc;
    return proc;

fail:
    vmm_destroy_address_space(proc->as);
    kfree(proc);
    return NULL;
}

/* ── proc_enter_usermode ─────────────────────────────────────────────────── */
__attribute__((noreturn))
void proc_enter_usermode(process_t *proc)
{
    proc_cur = proc;

    /* Point RSP0 in the TSS at this process's kernel stack so that hardware
     * interrupts arriving from ring 3 use the correct stack. */
    tss_set_kernel_stack(proc->kstack_top);

    /* Switch to the process address space */
    vmm_switch_address_space(proc->as);

    const uint64_t user_rip  = proc->user_rip;
    const uint64_t user_rsp  = proc->user_rsp;
    const uint64_t user_cs   = (uint64_t)(GDT_USER_CODE | 3);
    const uint64_t user_ss   = (uint64_t)(GDT_USER_DATA | 3);
    const uint64_t rflags    = 0x200ULL;  /* IF = 1, IOPL = 0 */

    /* Build an IRET frame on the current kernel stack and jump to ring 3. */
    __asm__ volatile(
        "push %0\n\t"   /* SS            */
        "push %1\n\t"   /* RSP (user)    */
        "push %2\n\t"   /* RFLAGS        */
        "push %3\n\t"   /* CS            */
        "push %4\n\t"   /* RIP (user)    */
        "iretq"
        :
        : "r"(user_ss), "r"(user_rsp), "r"(rflags),
          "r"(user_cs), "r"(user_rip)
        : "memory"
    );
    __builtin_unreachable();
}

/* ── proc_current ────────────────────────────────────────────────────────── */
process_t *proc_current(void) { return proc_cur; }

/* ── proc_exit ───────────────────────────────────────────────────────────── */
void proc_exit(process_t *proc, int code)
{
    if (!proc) return;
    proc->state     = PROC_STATE_ZOMBIE;
    proc->exit_code = code;
}

/* ── File descriptor helpers ─────────────────────────────────────────────── */

int proc_alloc_fd(process_t *proc, open_file_t *file)
{
    for (int i = 0; i < PROC_MAX_FDS; i++) {
        if (!proc->fds[i]) {
            proc->fds[i] = file;
            return i;
        }
    }
    return -1;
}

open_file_t *proc_get_fd(process_t *proc, int fd)
{
    if (fd < 0 || fd >= PROC_MAX_FDS) return NULL;
    return proc->fds[fd];
}

void proc_close_fd(process_t *proc, int fd)
{
    if (fd < 0 || fd >= PROC_MAX_FDS) return;
    open_file_t *f = proc->fds[fd];
    if (!f) return;

    /* Mark the pipe end closed so the other side sees EOF / EPIPE */
    if (f->type == FILE_TYPE_PIPE_READ && f->pipe)
        f->pipe->read_closed = true;
    else if (f->type == FILE_TYPE_PIPE_WRITE && f->pipe)
        f->pipe->write_closed = true;

    kfree(f);
    proc->fds[fd] = NULL;
}
