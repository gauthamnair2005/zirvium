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
#define SYS_DUP2    33    /* dup2(oldfd, newfd)                      */
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
#define SYS_PCI_READ  103   /* pci_read(index, info)                  */
#define SYS_NET_RESOLVE 104 /* net_resolve(target_ip, mac_out)        */

#define SYS_MOUSE_READ    122   /* mouse_read_event(ev) — read next mouse event */
#define SYS_DJ_SET_CURSOR 123   /* displayjet_set_cursor(x, y) */
#define SYS_MSLEEP       124   /* msleep(milliseconds) */
#define SYS_READ_KEYS    125   /* read_keys(event) — non-blocking keyboard event */
#define SYS_AUDIO_PLAY   126   /* audio_play(buf, frames) — submit PCM frames    */
#define SYS_AUDIO_VOLUME 127   /* audio_volume(vol) — set output volume 0-255    */

/* ── Message Queue syscalls ─────────────────────────────────────────────── */
#define SYS_MQ_CREATE   140  /* mq_create() → mq_id                              */
#define SYS_MQ_SEND     141  /* mq_send(mq_id, data, len, type) → 0 or -1       */
#define SYS_MQ_RECV     142  /* mq_recv(mq_id, buf, len, type_p, block) → len    */
#define SYS_MQ_DESTROY  143  /* mq_destroy(mq_id) → 0                           */

/* ── HPC / MPI syscalls ─────────────────────────────────────────────────── */
#define SYS_HPC_RANK    144  /* hpc_rank() → rank                               */
#define SYS_HPC_SIZE    145  /* hpc_size() → num processes                      */
#define SYS_HPC_BARRIER 146  /* hpc_barrier() → 0                               */
#define SYS_HPC_SEND    147  /* hpc_send(rank, data, len, tag) → 0              */
#define SYS_HPC_RECV    148  /* hpc_recv(rank, buf, len, tag) → actual_len      */
#define SYS_HPC_BCAST   149  /* hpc_bcast(data, len, root) → 0                  */
#define SYS_HPC_REDUCE  150  /* hpc_reduce(src, dst, len, op, root) → 0         */

/* ── ZirvGPU syscalls ────────────────────────────────────────────────────── */
#define SYS_GPU_MEM_ALLOC 130  /* gem_alloc(size) -> handle                     */
#define SYS_GPU_MEM_FREE  131  /* gem_free(handle)                              */
#define SYS_GPU_SUBMIT    132  /* gpu_submit(cmd_buf, size) -> status           */

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

/* ── Forward declarations ──────────────────────────────────────────────── */
struct process;

/* ── Public kernel API ───────────────────────────────────────────────────── */
void syscall_init(void);

uint64_t syscall_dispatch(uint64_t num,
                          uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6);

extern uint64_t syscall_kernel_stack_top;

/**
 * exec_enter_usermode — switch to user mode with a given return value in RAX.
 * Used by sys_exit and the fault handler to restore a saved (parent) process
 * after a child terminates.
 */
void __attribute__((noreturn))
exec_enter_usermode(struct process *proc, uint64_t retval);

#endif /* ZIRVIUM_KERNEL_SYSCALL_SYSCALL_H */