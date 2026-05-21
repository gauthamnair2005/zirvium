/* kernel/syscall/syscall.h
 * Zirvium Kernel — System call interface
 *
 * Syscall numbers match the Linux x86-64 ABI so that zirvlibc can target
 * both kernels with minimal glue.
 *
 * Calling convention (SYSCALL instruction, x86-64)
 *   RAX = syscall number
 *   RDI = arg1  RSI = arg2  RDX = arg3
 *   R10 = arg4  R8  = arg5  R9  = arg6
 *   (RCX is clobbered by SYSCALL; zirvlibc places arg4 in R10 instead)
 *
 *   On return: RAX = result, or negative errno on error.
 */
#ifndef ZIRVIUM_KERNEL_SYSCALL_SYSCALL_H
#define ZIRVIUM_KERNEL_SYSCALL_SYSCALL_H

#include <stdint.h>

/* ── Syscall numbers (Linux x86-64 ABI) ──────────────────────────────────── */
#define SYS_READ     0    /* read(fd, buf, count)                    */
#define SYS_WRITE    1    /* write(fd, buf, count)                   */
#define SYS_OPEN     2    /* open(path, flags)                       */
#define SYS_CLOSE    3    /* close(fd)                               */
#define SYS_MMAP     9    /* mmap(addr, len, prot, flags, fd, off)   */
#define SYS_MUNMAP   11   /* munmap(addr, len)                       */
#define SYS_BRK      12   /* brk(new_brk)  → returns actual brk     */
#define SYS_UPTIME   13   /* uptime()                                */
#define SYS_PIPE     22   /* pipe(fds[2])                            */
#define SYS_GETPID   39   /* getpid()                                */
#define SYS_EXECVE   59   /* execve(path, argv, envp)                */
#define SYS_EXIT     60   /* exit(status)  → does not return        */
#define SYS_WAIT4    61   /* wait4(pid, wstatus, options, rusage)    */
#define SYS_GETDENTS 78   /* getdents(fd, ents, count)               */
#define SYS_GETCWD   79   /* getcwd(buf, size)                       */
#define SYS_CHDIR    80   /* chdir(path)                             */
#define SYS_MKDIR      83   /* mkdir(path)                            */
#define SYS_RMDIR      84   /* rmdir(path)                            */
#define SYS_UNLINK     85   /* unlink(path)                           */
#define SYS_RENAME     86   /* rename(oldpath, newpath)                */
#define SYS_GETHOSTNAME 89   /* gethostname(buf, size)                 */
#define SYS_SETHOSTNAME 90   /* sethostname(name, len)                 */
#define SYS_GETDATETIME 96   /* getdatetime(dt)                       */
#define SYS_SETDATETIME 97   /* setdatetime(dt)                       */
#define SYS_GETTZ       98   /* gettz()                               */
#define SYS_SETTZ       99   /* settz(minutes)                        */
#define SYS_REBOOT     100   /* reboot()                              */
#define SYS_SHUTDOWN   101   /* shutdown()                             */
#define SYS_DNS_LOOKUP 102   /* dns_lookup(domain, result_ip)          */

/* ── Open flags (mirrored from user-space) ───────────────────────────────── */
#define O_CREAT  0x40

/* ── Negated errno values returned in RAX ────────────────────────────────── */
#define ESYS_EBADF   (-9)
#define ESYS_ENOMEM  (-12)
#define ESYS_EFAULT  (-14)
#define ESYS_EINVAL  (-22)
#define ESYS_EPIPE   (-32)
#define ESYS_ENOSYS  (-38)
#define ESYS_ENOENT  (-2)
#define ESYS_EACCES  (-13)
#define ESYS_ERANGE  (-34)

/* ── Public kernel API ───────────────────────────────────────────────────── */
void syscall_init(void);

uint64_t syscall_dispatch(uint64_t num,
                          uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6);

extern uint64_t syscall_kernel_stack_top;

#endif /* ZIRVIUM_KERNEL_SYSCALL_SYSCALL_H */