/* kernel/syscall/syscall.c
 * Zirvium Kernel — System call initialisation and dispatch
 */
#include "syscall.h"
#include "kernel/proc/process.h"
#include "kernel/console.h"
#include "kernel/mm/vmm.h"
#include "kernel/mm/pmm.h"
#include "kernel/ipc/pipe.h"
#include "kernel/loader/embedded.h"
#include "kernel/loader/elf.h"
#include "kernel/time/time.h"
#include "fs/mosix.h"
#include "arch/x64/cpu.h"
#include "arch/x64/gdt.h"
#include "drivers/serial/serial.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ── MSR addresses ───────────────────────────────────────────────────────── */
#define MSR_EFER   0xC0000080U
#define MSR_STAR   0xC0000081U
#define MSR_LSTAR  0xC0000082U
#define MSR_FMASK  0xC0000084U

/* ── Shared kernel syscall stack ─────────────────────────────────────────── */
#define SYSCALL_KSTACK_SIZE  (16u * 1024u)
static uint8_t syscall_kstack[SYSCALL_KSTACK_SIZE] __attribute__((aligned(16)));

uint64_t syscall_kernel_stack_top;

extern void syscall_entry(void);

/* ── syscall_init ────────────────────────────────────────────────────────── */
void syscall_init(void)
{
    syscall_kernel_stack_top = (uint64_t)(uintptr_t)
                               (syscall_kstack + SYSCALL_KSTACK_SIZE);

    uint64_t efer = rdmsr(MSR_EFER);
    efer |= (1ULL << 0);
    wrmsr(MSR_EFER, efer);

    uint64_t star = ((uint64_t)GDT_KERNEL_CODE << 32)
                  | ((uint64_t)GDT_KERNEL_DATA << 48);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);

    wrmsr(MSR_FMASK, (1ULL << 9));
}

/* ── Kernel heap wrappers ──────────────────────────────────────────────── */
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

static uint64_t sys_close(process_t *proc, int fd)
{
    if (!proc_get_fd(proc, fd)) return (uint64_t)(int64_t)ESYS_EBADF;
    proc_close_fd(proc, fd);
    return 0;
}

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
        if (rfd >= 0) proc_close_fd(proc, rfd); else kfree(rf);
        if (wfd >= 0) proc_close_fd(proc, wfd); else kfree(wf);
        pipe_destroy(p);
        return (uint64_t)(int64_t)ESYS_ENOMEM;
    }

    fds[0] = rfd;
    fds[1] = wfd;
    return 0;
}

static uint64_t sys_brk(process_t *proc, uint64_t new_brk)
{
    if (new_brk == 0 || new_brk <= proc->brk)
        return proc->brk;

    uint64_t old_top = (proc->brk  + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t new_top = (new_brk    + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

    for (uint64_t va = old_top; va < new_top; va += PAGE_SIZE) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return proc->brk;
        vmm_map_page(proc->as, va, phys,
                     PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NO_EXEC);
    }
    proc->brk = new_brk;
    return new_brk;
}

static uint64_t sys_mmap(process_t *proc,
                         uint64_t addr, size_t length,
                         int prot, int map_flags)
{
    (void)prot; (void)map_flags;
    if (length == 0) return (uint64_t)(int64_t)ESYS_EINVAL;

    size_t   pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NO_EXEC;

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

/* ── execve ──────────────────────────────────────────────────────────────── */
static uint64_t setup_user_stack(address_space_t *as, const char *path,
                                  char *const argv[], char *const envp[])
{
    (void)argv;
    (void)envp;

    uint64_t stack_base = PROC_USTACK_TOP - (uint64_t)PROC_USTACK_PAGES * PAGE_SIZE;
    for (int i = 0; i < PROC_USTACK_PAGES; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return 0;
        vmm_map_page(as, stack_base + (uint64_t)i * PAGE_SIZE, phys,
                     PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NO_EXEC);
    }

    uint64_t stack_top_page = PROC_USTACK_TOP - PAGE_SIZE;
    uint64_t stack_top_page_phys = vmm_virt_to_phys(as, stack_top_page);
    if (!stack_top_page_phys) return 0;
    char *stack_ka = (char *)PHYS_TO_VIRT(stack_top_page_phys);

    uint64_t sp = PAGE_SIZE;

    size_t path_len = strlen(path) + 1;
    sp -= path_len;
    memcpy(stack_ka + sp, path, path_len);
    uint64_t path_on_stack = stack_top_page + sp;

    if (sp % 8) sp -= (sp % 8);

    sp -= 8;
    *(uint64_t *)(stack_ka + sp) = 0;

    sp -= 8;
    *(uint64_t *)(stack_ka + sp) = 0;

    sp -= 8;
    *(uint64_t *)(stack_ka + sp) = (uint64_t)(uintptr_t)path_on_stack;

    sp -= 8;
    *(uint64_t *)(stack_ka + sp) = 1;
    uint64_t rsp = stack_top_page + sp;

    return rsp;
}

static void __attribute__((noreturn))
exec_enter_usermode(process_t *proc)
{
    extern void tss_set_kernel_stack(uint64_t rsp0);
    tss_set_kernel_stack(proc->kstack_top);
    vmm_switch_address_space(proc->as);

    const uint64_t user_rip = proc->user_rip;
    const uint64_t user_rsp = proc->user_rsp;
    const uint64_t user_cs  = (uint64_t)(GDT_USER_CODE | 3);
    const uint64_t user_ss  = (uint64_t)(GDT_USER_DATA | 3);
    const uint64_t rflags   = 0x202ULL;

    __asm__ volatile(
        "push %0\n\t"
        "push %1\n\t"
        "push %2\n\t"
        "push %3\n\t"
        "push %4\n\t"
        "iretq"
        :: "r"(user_ss), "r"(user_rsp), "r"(rflags),
           "r"(user_cs), "r"(user_rip)
        : "memory"
    );
    __builtin_unreachable();
}

static uint64_t sys_execve(process_t *proc,
                           const char *path,
                           char *const argv[],
                           char *const envp[])
{
    (void)argv;
    (void)envp;
    if (!path) return (uint64_t)(int64_t)ESYS_EFAULT;

    size_t bin_size;
    const void *binary = embedded_find(path, &bin_size);
    if (!binary) return (uint64_t)(int64_t)ESYS_ENOENT;

    address_space_t *new_as = vmm_create_address_space();
    if (!new_as) return (uint64_t)(int64_t)ESYS_ENOMEM;

    uint64_t new_entry;
    if (!elf_load_into_as(new_as, binary, &new_entry)) {
        vmm_destroy_address_space(new_as);
        return (uint64_t)(int64_t)ESYS_EINVAL;
    }

    uint64_t new_rsp = setup_user_stack(new_as, path, NULL, NULL);
    if (!new_rsp) {
        vmm_destroy_address_space(new_as);
        return (uint64_t)(int64_t)ESYS_ENOMEM;
    }

    address_space_t *old_as = proc->as;
    proc->as = new_as;
    proc->user_rip   = new_entry;
    proc->user_rsp   = new_rsp;
    proc->brk        = PROC_HEAP_START;
    proc->mmap_cursor = PROC_MMAP_START;
    proc->state      = PROC_STATE_RUNNING;

    vmm_destroy_address_space(old_as);

    exec_enter_usermode(proc);
    __builtin_unreachable();
}

/* ── wait4 ───────────────────────────────────────────────────────────────── */
static uint64_t sys_wait4(process_t *proc,
                          int pid, int *wstatus, int options, void *rusage)
{
    (void)options;
    (void)rusage;

    process_t *child = proc_find_child(proc, (uint32_t)(pid > 0 ? (uint32_t)pid : 0));
    if (!child) return (uint64_t)(int64_t)(-10); /* -ECHILD */
    if (child->state != PROC_STATE_ZOMBIE) return 0;

    if (wstatus) {
        int status = 0;
        status |= (child->exit_code & 0xFF) << 8;
        *wstatus = status;
    }

    uint32_t child_pid = child->pid;
    child->state = PROC_STATE_UNUSED;
    return (uint64_t)child_pid;
}

/* ── time ────────────────────────────────────────────────────────────────── */
static uint64_t sys_uptime(process_t *proc)
{
    (void)proc;
    return time_uptime_seconds();
}

static uint64_t sys_getdatetime(process_t *proc, struct datetime *buf)
{
    (void)proc;
    if (!buf) return (uint64_t)(int64_t)ESYS_EFAULT;
    struct datetime dt;
    if (time_get(&dt) < 0) return (uint64_t)(int64_t)ESYS_EINVAL;
    *buf = dt;
    return 0;
}

static uint64_t sys_setdatetime(process_t *proc, const struct datetime *dt)
{
    (void)proc;
    if (!dt) return (uint64_t)(int64_t)ESYS_EFAULT;
    if (time_set(dt) < 0) return (uint64_t)(int64_t)ESYS_EINVAL;
    return 0;
}

static uint64_t sys_gettz(process_t *proc)
{
    (void)proc;
    return (uint64_t)(int64_t)time_get_tz();
}

static uint64_t sys_settz(process_t *proc, int minutes)
{
    (void)proc;
    time_set_tz(minutes);
    return 0;
}

/* ── getdents ────────────────────────────────────────────────────────────── */
struct dirent {
    uint64_t d_ino;
    char     d_name[256];
};

static uint64_t sys_getdents(process_t *proc, int fd,
                             struct dirent *ents, int count)
{
    open_file_t *f = proc_get_fd(proc, fd);
    if (!f) return (uint64_t)(int64_t)ESYS_EBADF;
    if (f->type != FILE_TYPE_VFS || !f->vnode)
        return (uint64_t)(int64_t)ESYS_EINVAL;
    if (f->vnode->type != VNODE_DIR)
        return (uint64_t)(int64_t)ESYS_EINVAL;
    if (!f->vnode->ops || !f->vnode->ops->readdir)
        return (uint64_t)(int64_t)ESYS_EINVAL;

    int written = 0;
    for (int i = 0; i < count; i++) {
        char name[256];
        if (f->vnode->ops->readdir(f->vnode, (uint32_t)i,
                                    name, sizeof(name)) < 0)
            break;
        ents[i].d_ino = f->vnode->inode;
        size_t nlen = strlen(name) + 1;
        if (nlen > sizeof(ents[i].d_name))
            nlen = sizeof(ents[i].d_name);
        memcpy(ents[i].d_name, name, nlen);
        written++;
    }
    return (uint64_t)written;
}

/* ── getcwd ──────────────────────────────────────────────────────────────── */
static uint64_t sys_getcwd(process_t *proc, char *buf, size_t size)
{
    if (!buf) return (uint64_t)(int64_t)ESYS_EFAULT;

    const char *cwd = proc_get_cwd(proc);
    size_t len = strlen(cwd) + 1;

    if (len > size) return (uint64_t)(int64_t)ESYS_ERANGE;

    memcpy(buf, cwd, len);
    return (uint64_t)(len - 1);
}

/* ── chdir ───────────────────────────────────────────────────────────────── */
static uint64_t sys_chdir(process_t *proc, const char *path)
{
    if (!path) return (uint64_t)(int64_t)ESYS_EFAULT;

    vnode_t *v = vfs_lookup(path);
    if (!v) return (uint64_t)(int64_t)ESYS_ENOENT;
    if (v->type != VNODE_DIR && v->type != VNODE_MOUNTPT)
        return (uint64_t)(int64_t)ESYS_EINVAL;

    proc_set_cwd(proc, path);
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
    case SYS_UPTIME:
        return sys_uptime(proc);
    case SYS_PIPE:
        return sys_pipe(proc, (int *)(uintptr_t)a1);
    case SYS_GETPID:
        return (uint64_t)proc->pid;
    case SYS_EXECVE:
        return sys_execve(proc,
                          (const char *)(uintptr_t)a1,
                          (char *const *)(uintptr_t)a2,
                          (char *const *)(uintptr_t)a3);
    case SYS_EXIT:
        proc_exit(proc, (int)a1);
        __asm__ volatile("cli");
        for (;;) __asm__ volatile("hlt");
        return 0;
    case SYS_WAIT4:
        return sys_wait4(proc, (int)a1,
                         (int *)(uintptr_t)a2, (int)a3,
                         (void *)(uintptr_t)a4);
    case SYS_GETDENTS:
        return sys_getdents(proc, (int)a1,
                            (struct dirent *)(uintptr_t)a2, (int)a3);
    case SYS_GETCWD:
        return sys_getcwd(proc,
                          (char *)(uintptr_t)a1, (size_t)a2);
    case SYS_CHDIR:
        return sys_chdir(proc, (const char *)(uintptr_t)a1);
    case SYS_GETDATETIME:
        return sys_getdatetime(proc, (struct datetime *)(uintptr_t)a1);
    case SYS_SETDATETIME:
        return sys_setdatetime(proc, (const struct datetime *)(uintptr_t)a1);
    case SYS_GETTZ:
        return sys_gettz(proc);
    case SYS_SETTZ:
        return sys_settz(proc, (int)a1);
    case SYS_REBOOT:
        serial_puts(SERIAL_COM1, "[syscall] rebooting via ACPI reset register\n");
        outb(0xCF9, 0x0E);  /* ACPI full reset (RST_CPU | SYS_RST) */
        outb(0xCF9, 0x06);  /* ACPI hard reset (SYS_RST only) */
        outb(0x64, 0xFE);   /* legacy keyboard controller reset */
        for (;;) __asm__ volatile("hlt");
        return 0;
    case SYS_SHUTDOWN:
        serial_puts(SERIAL_COM1, "[syscall] shutdown via QEMU/Bochs exit\n");
        outw(0x604, 0x2000);  /* QEMU */
        outw(0xB004, 0x2000); /* Bochs */
        for (;;) __asm__ volatile("hlt");
        return 0;
    default:
        return (uint64_t)(int64_t)ESYS_ENOSYS;
    }
}