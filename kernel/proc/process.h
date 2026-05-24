/* kernel/proc/process.h
 * Zirvium Kernel — Process management
 *
 * Defines the kernel-side process descriptor (process_t), the per-process
 * open-file table (open_file_t), and the API used by the syscall layer to
 * create processes, enter user mode, and manage file descriptors.
 *
 * Virtual address layout for each user process
 * ─────────────────────────────────────────────
 *   0x0000000000001000 – 0x000000FFFFFFFFFF  user code / data  (loaded later)
 *   0x0000100000000000                        heap base  (PROC_HEAP_START)
 *   0x0000200000000000                        anonymous mmap region start
 *   0x0000700000000000                        user stack top  (PROC_USTACK_TOP,
 *                                             grows downward, PROC_USTACK_PAGES
 *                                             × 4 KiB pre-mapped)
 *   0xFFFF800000000000 –                      kernel (shared, not user-writable)
 */
#ifndef ZIRVIUM_KERNEL_PROC_PROCESS_H
#define ZIRVIUM_KERNEL_PROC_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/mm/vmm.h"
#include "kernel/ipc/pipe.h"
#include "fs/mosix.h"

/* ── User virtual memory constants ────────────────────────────────────────── */
#define PROC_USTACK_TOP    0x0000700000000000ULL   /* user stack ceiling VA */
#define PROC_USTACK_PAGES  16                       /* 64 KiB user stack     */
#define PROC_HEAP_START    0x0000100000000000ULL    /* initial brk value     */
#define PROC_MMAP_START    0x0000200000000000ULL    /* first mmap VA         */

/* ── File descriptor limits ────────────────────────────────────────────────── */
#define PROC_MAX_FDS  64

/* ── Open-file types ───────────────────────────────────────────────────────── */
typedef enum {
    FILE_TYPE_NONE       = 0,
    FILE_TYPE_VFS,         /* regular VFS node (device, regular file, etc.) */
    FILE_TYPE_PIPE_READ,   /* read end of a pipe                            */
    FILE_TYPE_PIPE_WRITE,  /* write end of a pipe                           */
} file_type_t;

typedef struct open_file {
    file_type_t  type;
    union {
        vnode_t *vnode;  /* FILE_TYPE_VFS        */
        pipe_t  *pipe;   /* FILE_TYPE_PIPE_{R,W} */
    };
    uint64_t offset;   /* byte offset for VFS reads/writes */
    int      flags;    /* O_* flags from sys_open          */
} open_file_t;

/* ── Process states ─────────────────────────────────────────────────────────── */
typedef enum {
    PROC_STATE_UNUSED  = 0,
    PROC_STATE_RUNNING,
    PROC_STATE_BLOCKED,
    PROC_STATE_ZOMBIE,
} proc_state_t;

/* ── Kernel stack size per process ──────────────────────────────────────────── */
#define PROC_KSTACK_PAGES  4   /* 16 KiB kernel stack */

/* ── Globals exposed by process.c ──────────────────────────────────────────── */
extern struct process *proc_list;
extern struct process *proc_cur;

/* ── Process descriptor ─────────────────────────────────────────────────────── */
typedef struct process {
    uint32_t         pid;
    proc_state_t     state;

    /* Virtual memory */
    address_space_t *as;
    uint64_t         user_rip;     /* entry point / last saved user RIP */
    uint64_t         user_rsp;     /* last saved user RSP               */
    uint64_t         kstack_top;   /* top of kernel stack (used by TSS) */
    uint64_t         kernel_rsp;   /* saved kernel RSP for preemptive switch */

    /* Heap / mmap state */
    uint64_t         brk;          /* current program break             */
    uint64_t         mmap_cursor;  /* next VA for anonymous mmap        */

    /* Open file descriptor table */
    open_file_t     *fds[PROC_MAX_FDS];

    /* Exit state */
    int              exit_code;

    /* Saved parent state — used by execve to restore caller after child exits */
    address_space_t *saved_as;       /* caller's address space (NULL = no saved state) */
    uint64_t         saved_rip;      /* caller's user RIP */
    uint64_t         saved_rsp;      /* caller's user RSP */
    uint64_t         user_rax;       /* RAX to pass to user mode on next entry */

    /* Parent / child tracking */
    struct process  *parent;       /* parent process, NULL for PID 1   */
    struct process  *child;        /* first child in singly-linked list */
    struct process  *next_sibling; /* next sibling in parent's list    */

    /* Current working directory */
    char             cwd[256];

    /* HPC fields */
    int              hpc_rank;
    int              hpc_enabled;
    int              hpc_barrier_count;

    /* Intrusive linked list of all processes (scheduling) */
    struct process  *next;
} process_t;

/* ── Public API ─────────────────────────────────────────────────────────────── */

/**
 * proc_init — initialise the process subsystem.  Must be called once at boot
 * before any other proc_* function.
 */
void proc_init(void);

/**
 * proc_create — allocate a new process with its own address space.
 *
 * @entry_virt: virtual address at which the process will begin execution.
 *              The caller is responsible for mapping user code into the new
 *              address space via vmm_map_page() before calling
 *              proc_enter_usermode().
 *
 * Returns the new process_t, or NULL on failure.
 */
process_t *proc_create(uint64_t entry_virt);

/**
 * proc_enter_usermode — switch irreversibly to user mode and begin executing
 * @proc.  Must be called with interrupts disabled.  Does not return.
 */
__attribute__((noreturn))
void proc_enter_usermode(process_t *proc);

/** proc_current — return the currently executing process (NULL in idle). */
process_t *proc_current(void);

/**
 * proc_exit — mark @proc as a zombie with the given exit code.
 * The scheduler (TODO) is responsible for reaping zombie processes.
 */
void proc_exit(process_t *proc, int code);

/**
 * proc_alloc_fd — install @file in the first free descriptor slot of @proc.
 * Returns the file descriptor number, or -1 if the table is full.
 */
int proc_alloc_fd(process_t *proc, open_file_t *file);

/**
 * proc_get_fd — look up descriptor @fd in @proc.
 * Returns the open_file_t, or NULL if @fd is out of range or unused.
 */
open_file_t *proc_get_fd(process_t *proc, int fd);

/**
 * proc_close_fd — close descriptor @fd: marks the pipe end as closed (if
 * applicable) and frees the open_file_t.
 */
void proc_close_fd(process_t *proc, int fd);

/**
 * proc_set_cwd — set the current working directory of @proc.
 */
void proc_set_cwd(process_t *proc, const char *path);

/**
 * proc_get_cwd — get the current working directory of @proc.
 * Returns a pointer to the internal buffer.
 */
const char *proc_get_cwd(process_t *proc);

/**
 * proc_link_child — add @child to @parent's list of children.
 */
void proc_link_child(process_t *parent, process_t *child);

/**
 * proc_find_child — find a child of @proc by PID (0 = any child).
 * Returns the first matching child, or NULL.
 */
process_t *proc_find_child(process_t *proc, uint32_t pid);

#endif /* ZIRVIUM_KERNEL_PROC_PROCESS_H */
