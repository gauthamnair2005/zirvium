/* kernel/syscall/syscall.h
 * Zirvium Kernel — System call interface
 *
 * Syscall numbers match the Linux x86-64 ABI so that zirvlibc can target
 * both kernels with minimal glue.
 *
 * Calling convention (SYSCALL instruction, x86-64)
 * ─────────────────────────────────────────────────
 *   RAX = syscall number
 *   RDI = arg1  RSI = arg2  RDX = arg3
 *   R10 = arg4  R8  = arg5  R9  = arg6
 *   (RCX is clobbered by SYSCALL; zirvlibc places arg4 in R10 instead)
 *
 *   On return: RAX = result, or negative errno on error.
 *
 * Adding a new syscall
 * ─────────────────────
 *   1. Define its SYS_* number here.
 *   2. Add a case in syscall_dispatch() in syscall.c.
 *   3. Document the prototype in this header.
 */
#ifndef ZIRVIUM_KERNEL_SYSCALL_SYSCALL_H
#define ZIRVIUM_KERNEL_SYSCALL_SYSCALL_H

#include <stdint.h>

/* ── Syscall numbers ─────────────────────────────────────────────────────── */
#define SYS_READ     0    /* read(fd, buf, count)                    */
#define SYS_WRITE    1    /* write(fd, buf, count)                   */
#define SYS_OPEN     2    /* open(path, flags)                       */
#define SYS_CLOSE    3    /* close(fd)                               */
#define SYS_MMAP     9    /* mmap(addr, len, prot, flags, fd, off)   */
#define SYS_MUNMAP   11   /* munmap(addr, len)                       */
#define SYS_BRK      12   /* brk(new_brk)  → returns actual brk     */
#define SYS_PIPE     22   /* pipe(fds[2])                            */
#define SYS_GETPID   39   /* getpid()                                */
#define SYS_EXIT     60   /* exit(status)  → does not return        */

/* ── Negated errno values returned in RAX ────────────────────────────────── */
#define ESYS_EBADF   (-9)
#define ESYS_ENOMEM  (-12)
#define ESYS_EFAULT  (-14)
#define ESYS_EINVAL  (-22)
#define ESYS_EPIPE   (-32)
#define ESYS_ENOSYS  (-38)

/* ── Public kernel API ───────────────────────────────────────────────────── */

/**
 * syscall_init — configure the CPU for the SYSCALL/SYSRET instruction pair
 * and install the low-level entry stub.
 *
 * Writes MSRs: EFER (sets SCE), STAR (segment selectors), LSTAR (handler
 * address), FMASK (clears IF on entry so the handler runs with interrupts
 * disabled).
 *
 * Must be called after gdt_init() and vmm_init().
 */
void syscall_init(void);

/**
 * syscall_dispatch — C-level syscall router, called from
 * arch/x64/syscall_entry.asm.
 *
 * @num       syscall number (SYS_*)
 * @a1–@a6   arguments in the order described above
 *
 * Returns the result value placed in RAX for userspace.
 */
uint64_t syscall_dispatch(uint64_t num,
                          uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6);

/* Exported to arch/x64/syscall_entry.asm — top of the shared kernel stack */
extern uint64_t syscall_kernel_stack_top;

#endif /* ZIRVIUM_KERNEL_SYSCALL_SYSCALL_H */
