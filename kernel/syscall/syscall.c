#include <kernel/syscall.h>
#include <kernel/process.h>
#include <kernel/vfs.h>
#include <kernel/kernel.h>

static int64_t sys_read(int fd, void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return 0;
}

static int64_t sys_write(int fd, const void *buf, size_t count) {
    if (fd == 1 || fd == 2) {
        const char *str = (const char*)buf;
        for (size_t i = 0; i < count; i++) {
            kprintf("%c", str[i]);
        }
        return count;
    }
    return -1;
}

static int64_t sys_open(const char *path, int flags) {
    (void)path; (void)flags;
    return -1;
}

static int64_t sys_close(int fd) {
    (void)fd;
    return 0;
}

static int64_t sys_fork(void) {
    return -1;
}

static int64_t sys_exec(const char *path, char *const argv[]) {
    (void)path; (void)argv;
    return -1;
}

static int64_t sys_exit(int status) {
    proc_exit(status);
    return 0;
}

static int64_t sys_getpid(void) {
    process_t *proc = proc_current();
    return proc ? proc->pid : -1;
}

static int64_t sys_kill(pid_t pid, int sig) {
    (void)pid; (void)sig;
    return -1;
}

static uint64_t syscall_count = 0;

void syscall_init(void) {
    syscall_count = 0;
    kprintf("System calls: UNIX-like interface ready (9 syscalls)\n");
}

int64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, 
                        uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg4; (void)arg5;
    syscall_count++;
    
    switch (syscall_num) {
        case SYS_READ:
            return sys_read((int)arg1, (void*)(uintptr_t)arg2, (size_t)arg3);
        case SYS_WRITE:
            return sys_write((int)arg1, (const void*)(uintptr_t)arg2, (size_t)arg3);
        case SYS_OPEN:
            return sys_open((const char*)(uintptr_t)arg1, (int)arg2);
        case SYS_CLOSE:
            return sys_close((int)arg1);
        case SYS_FORK:
            return sys_fork();
        case SYS_EXEC:
            return sys_exec((const char*)(uintptr_t)arg1, (char *const *)(uintptr_t)arg2);
        case SYS_EXIT:
            return sys_exit((int)arg1);
        case SYS_GETPID:
            return sys_getpid();
        case SYS_KILL:
            return sys_kill((pid_t)arg1, (int)arg2);
        default:
            return -1;
    }
}
