/* kernel/syscall/syscall.c
 * Zirvium Kernel — System call initialisation and dispatch
 *
 * syscall_init()   — writes the necessary x86-64 MSRs (EFER/STAR/LSTAR/FMASK)
 *                    so the SYSCALL instruction lands in our assembly stub.
 * syscall_dispatch() — C handler called by the stub; routes each call to its
 *                    implementation and returns the result in RAX.
 *
 * STAR MSR encoding (with swapped GDT user segments):
 *   GDT layout  0x00=null  0x08=kcode  0x10=kdata  0x18=udata  0x20=ucode
 *
 *   STAR[47:32] = GDT_KERNEL_CODE (0x08)
 *     → SYSCALL: CS = 0x08, SS = 0x08+8 = 0x10 (kernel data) ✓
 *
 *   STAR[63:48] = GDT_KERNEL_DATA (0x10)
 *     → SYSRETQ: CS = 0x10+16 = 0x20 (user code, RPL forced 3) ✓
 *                SS = 0x10+8  = 0x18 (user data, RPL forced 3) ✓
 */
#include "syscall.h"
#include "kernel/proc/process.h"
#include "kernel/mm/vmm.h"
#include "kernel/mm/pmm.h"
#include "kernel/ipc/pipe.h"
#include "fs/mosix.h"
#include "arch/x64/cpu.h"
#include "arch/x64/gdt.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── MSR addresses ───────────────────────────────────────────────────────── */
#define MSR_EFER   0xC0000080U   /* Extended Feature Enable Register */
#define MSR_STAR   0xC0000081U   /* Syscall/Sysret segment selectors */
#define MSR_LSTAR  0xC0000082U   /* 64-bit SYSCALL target RIP        */
#define MSR_FMASK  0xC0000084U   /* RFLAGS mask on SYSCALL entry     */

/* ── Shared kernel syscall stack ─────────────────────────────────────────── */
#define SYSCALL_KSTACK_SIZE  (16u * 1024u)
static uint8_t syscall_kstack[SYSCALL_KSTACK_SIZE] __attribute__((aligned(16)));

/* Referenced by arch/x64/syscall_entry.asm */
uint64_t syscall_kernel_stack_top;

/* Assembly entry stub (arch/x64/syscall_entry.asm) */
extern void syscall_entry(void);

/* ── syscall_init ────────────────────────────────────────────────────────── */
void syscall_init(void)
{
    syscall_kernel_stack_top = (uint64_t)(uintptr_t)
                               (syscall_kstack + SYSCALL_KSTACK_SIZE);

    /* Enable SYSCALL/SYSRET: set EFER.SCE (bit 0) */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= (1ULL << 0);
    wrmsr(MSR_EFER, efer);

    /* STAR: kernel CS in [47:32], SYSRET base in [63:48] */
    uint64_t star = ((uint64_t)GDT_KERNEL_CODE << 32)
                  | ((uint64_t)GDT_KERNEL_DATA << 48);
    wrmsr(MSR_STAR, star);

    /* LSTAR: 64-bit SYSCALL entry address */
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);

    /* FMASK: clear IF (bit 9) so interrupts are disabled inside syscalls */
    wrmsr(MSR_FMASK, (1ULL << 9));
}

/* ── Kernel heap wrappers ────────────────────────────────────────────────── */
extern void *kmalloc(size_t size, unsigned int flags);
extern void  kfree(void *ptr);

static open_file_t *alloc_file(file_type_t type)
{
    open_file_t *f = (open_file_t *)kmalloc(sizeof(open_file_t), 0);
    if (!f) return NULL;
    memset(f, 0, sizeof(open_file_t));
    f->type = type;
    return f;
}

/* ── Individual syscall handlers ──────────────────────────────────────────── */

/* read(fd, buf, count) */
static uint64_t sys_read(process_t *proc,
                         int fd, void *buf, size_t count)
{
    open_file_t *f = proc_get_fd(proc, fd);
    if (!f) return (uint64_t)(int64_t)ESYS_EBADF;

    switch (f->type) {
    case FILE_TYPE_VFS:
        if (!f->vnode || !f->vnode->ops || !f->vnode->ops->read)
            return (uint64_t)(int64_t)ESYS_EINVAL;
        {
            int n = f->vnode->ops->read(f->vnode, buf, count, f->offset);
            if (n > 0) f->offset += (uint64_t)n;
            return (uint64_t)(int64_t)n;
        }
    case FILE_TYPE_PIPE_READ:
        return (uint64_t)(int64_t)pipe_read(f->pipe, buf, count);
    default:
        return (uint64_t)(int64_t)ESYS_EBADF;
    }
}

/* write(fd, buf, count) */
static uint64_t sys_write(process_t *proc,
                          int fd, const void *buf, size_t count)
{
    open_file_t *f = proc_get_fd(proc, fd);
    if (!f) return (uint64_t)(int64_t)ESYS_EBADF;

    switch (f->type) {
    case FILE_TYPE_VFS:
        if (!f->vnode || !f->vnode->ops || !f->vnode->ops->write)
            return (uint64_t)(int64_t)ESYS_EINVAL;
        {
            int n = f->vnode->ops->write(f->vnode, buf, count, f->offset);
            if (n > 0) f->offset += (uint64_t)n;
            return (uint64_t)(int64_t)n;
        }
    case FILE_TYPE_PIPE_WRITE:
        return (uint64_t)(int64_t)pipe_write(f->pipe, buf, count);
    default:
        return (uint64_t)(int64_t)ESYS_EBADF;
    }
}

/* open(path, flags)
 * NOTE: @path is a user virtual address.  Because the kernel runs with the
 * process CR3 loaded, user VA is directly dereferenceable here. */
static uint64_t sys_open(process_t *proc,
                         const char *path, int flags)
{
    if (!path) return (uint64_t)(int64_t)ESYS_EFAULT;

    vnode_t *v = vfs_lookup(path);
    if (!v) return (uint64_t)(int64_t)ESYS_EINVAL;

    open_file_t *f = alloc_file(FILE_TYPE_VFS);
    if (!f) return (uint64_t)(int64_t)ESYS_ENOMEM;
    f->vnode  = v;
    f->offset = 0;
    f->flags  = flags;

    int fd = proc_alloc_fd(proc, f);
    if (fd < 0) { kfree(f); return (uint64_t)(int64_t)ESYS_ENOMEM; }
    return (uint64_t)fd;
}

/* close(fd) */
static uint64_t sys_close(process_t *proc, int fd)
{
    if (!proc_get_fd(proc, fd)) return (uint64_t)(int64_t)ESYS_EBADF;
    proc_close_fd(proc, fd);
    return 0;
}

/* pipe(fds[2])
 * Creates a unidirectional pipe.  fds[0] is the read end, fds[1] the write
 * end.  @fds is a user virtual address (directly accessible, see sys_open). */
static uint64_t sys_pipe(process_t *proc, int *fds)
{
    if (!fds) return (uint64_t)(int64_t)ESYS_EFAULT;

    pipe_t *p = pipe_create();
    if (!p) return (uint64_t)(int64_t)ESYS_ENOMEM;

    open_file_t *rf = alloc_file(FILE_TYPE_PIPE_READ);
    open_file_t *wf = alloc_file(FILE_TYPE_PIPE_WRITE);
    if (!rf || !wf) {
        if (rf) kfree(rf);
        if (wf) kfree(wf);
        pipe_destroy(p);
        return (uint64_t)(int64_t)ESYS_ENOMEM;
    }
    rf->pipe = wf->pipe = p;

    int rfd = proc_alloc_fd(proc, rf);
    int wfd = proc_alloc_fd(proc, wf);
    if (rfd < 0 || wfd < 0) {
        if (rfd >= 0) { proc->fds[rfd] = NULL; }
        if (wfd >= 0) { proc->fds[wfd] = NULL; }
        kfree(rf); kfree(wf); pipe_destroy(p);
        return (uint64_t)(int64_t)ESYS_ENOMEM;
    }

    fds[0] = rfd;
    fds[1] = wfd;
    return 0;
}

/* brk(new_brk) → returns the resulting brk value
 * Extends (or queries) the process heap.  new_brk == 0 queries current brk. */
static uint64_t sys_brk(process_t *proc, uint64_t new_brk)
{
    if (new_brk == 0 || new_brk <= proc->brk)
        return proc->brk;

    /* Map pages between the old and new break */
    uint64_t old_top = (proc->brk  + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t new_top = (new_brk    + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

    for (uint64_t va = old_top; va < new_top; va += PAGE_SIZE) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return proc->brk;  /* OOM: return unchanged brk */
        vmm_map_page(proc->as, va, phys,
                     PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NO_EXEC);
    }
    proc->brk = new_brk;
    return new_brk;
}

/* mmap(addr, len, prot, flags, fd, off)
 * Simplified implementation: anonymous-only, read/write, no file backing. */
static uint64_t sys_mmap(process_t *proc,
                         uint64_t addr, size_t length,
                         int prot, int map_flags)
{
    (void)prot; (void)map_flags;
    if (length == 0) return (uint64_t)(int64_t)ESYS_EINVAL;

    size_t   pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NO_EXEC;

    /* Use the per-process mmap cursor when no address hint is given */
    if (addr == 0) {
        addr = proc->mmap_cursor;
        proc->mmap_cursor += (uint64_t)pages * PAGE_SIZE;
    }

    for (size_t i = 0; i < pages; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return (uint64_t)(int64_t)ESYS_ENOMEM;
        vmm_map_page(proc->as, addr + (uint64_t)i * PAGE_SIZE, phys, flags);
    }
    return addr;
}

/* munmap(addr, len) */
static uint64_t sys_munmap(process_t *proc, uint64_t addr, size_t length)
{
    if (length == 0) return (uint64_t)(int64_t)ESYS_EINVAL;

    size_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < pages; i++) {
        uint64_t va   = addr + (uint64_t)i * PAGE_SIZE;
        uint64_t phys = vmm_virt_to_phys(proc->as, va);
        if (phys) {
            pmm_free_page(phys);
            vmm_unmap_page(proc->as, va);
        }
    }
    return 0;
}

/* ── syscall_dispatch ────────────────────────────────────────────────────── */
uint64_t syscall_dispatch(uint64_t num,
                          uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6)
{
    (void)a5; (void)a6;

    process_t *proc = proc_current();
    if (!proc) return (uint64_t)(int64_t)ESYS_EINVAL;

    switch ((int)num) {
    case SYS_READ:
        return sys_read(proc, (int)a1,
                        (void *)(uintptr_t)a2, (size_t)a3);
    case SYS_WRITE:
        return sys_write(proc, (int)a1,
                         (const void *)(uintptr_t)a2, (size_t)a3);
    case SYS_OPEN:
        return sys_open(proc, (const char *)(uintptr_t)a1, (int)a2);
    case SYS_CLOSE:
        return sys_close(proc, (int)a1);
    case SYS_MMAP:
        return sys_mmap(proc, a1, (size_t)a2, (int)a3, (int)a4);
    case SYS_MUNMAP:
        return sys_munmap(proc, a1, (size_t)a2);
    case SYS_BRK:
        return sys_brk(proc, a1);
    case SYS_PIPE:
        return sys_pipe(proc, (int *)(uintptr_t)a1);
    case SYS_GETPID:
        return (uint64_t)proc->pid;
    case SYS_EXIT:
        proc_exit(proc, (int)a1);
        /* TODO: invoke scheduler to run next process */
        __asm__ volatile("cli");
        for (;;) __asm__ volatile("hlt");
        return 0;   /* unreachable */
    default:
        return (uint64_t)(int64_t)ESYS_ENOSYS;
    }
}
