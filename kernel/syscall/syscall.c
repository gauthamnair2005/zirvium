#include <kernel/syscall.h>
#include <kernel/process.h>
#include <kernel/vfs.h>
#include <kernel/kernel.h>
#include <kernel/admin.h>
#include <kernel/error.h>
#include <kernel/displayjet.h>

static int64_t sys_read(int fd, void *buf, size_t count) {
    if (!buf || count == 0) {
        error_set_last(E_INVAL);
        return E_INVAL;
    }
    if (fd < 0) {
        error_set_last(E_BADF);
        return E_BADF;
    }
    /* TODO: Implement file reading */
    (void)fd;
    error_set_last(E_NOTIMPL);
    return E_NOTIMPL;
}

static int64_t sys_write(int fd, const void *buf, size_t count) {
    if (!buf || count == 0) {
        error_set_last(E_INVAL);
        return E_INVAL;
    }
    
    if (fd == 1 || fd == 2) {
        if (count > 4096) count = 4096; /* Limit write size */
        
        const char *str = (const char*)buf;
        for (size_t i = 0; i < count; i++) {
            kprintf("%c", str[i]);
        }
        return (int64_t)count;
    }
    
    error_set_last(E_BADF);
    return E_BADF;
}

static int64_t sys_open(const char *path, int flags) {
    if (!path) {
        error_set_last(E_INVAL);
        return E_INVAL;
    }
    (void)flags;
    /* TODO: Implement file opening */
    error_set_last(E_NOTIMPL);
    return E_NOTIMPL;
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
    (void)sig;
    
    process_t *proc = proc_current();
    if (!proc) {
        error_set_last(E_GENERIC);
        return E_GENERIC;
    }
    
    if (pid < 0) {
        error_set_last(E_INVAL);
        return E_INVAL;
    }
    
    /* Check if caller has permission to kill the target */
    if (pid != proc->pid) {
        /* Killing other processes requires KILL_ANY capability */
        if (!admin_check_capability(proc->pid, ADMIN_CAP_KILL_ANY)) {
            admin_log_access(proc->pid, "kill()", 0);
            kprintf("Permission denied: PID %d cannot kill PID %d\n", proc->pid, pid);
            error_set_last(E_PERM);
            return E_PERM;
        }
        admin_log_access(proc->pid, "kill()", 1);
    }
    
    /* TODO: Implement actual kill logic */
    error_set_last(E_NOTIMPL);
    return E_NOTIMPL;
}

static int64_t sys_admin(int command) {
    process_t *proc = proc_current();
    if (!proc) return -1;
    
    switch (command) {
        case ADMIN_CMD_ELEVATE: {
            /* Request elevation to admin level */
            /* In production, this would require authentication */
            if (admin_is_admin(proc->pid)) {
                kprintf("Admin: PID %d already has admin privileges\n", proc->pid);
                return 0;
            }
            return admin_elevate(proc->pid, ADMIN_LEVEL_ADMIN);
        }
        
        case ADMIN_CMD_DROP: {
            /* Drop all privileges */
            return admin_drop(proc->pid);
        }
        
        case ADMIN_CMD_CHECK: {
            /* Return current admin level */
            return admin_get_level(proc->pid);
        }
        
        case ADMIN_CMD_LIST_CAPS: {
            /* Return current capabilities */
            return admin_get_capabilities(proc->pid);
        }
        
        default:
            return -1;
    }
}

static uint64_t syscall_count = 0;

void syscall_init(void) {
    syscall_count = 0;
    kprintf("System calls: UNIX-like interface ready (17 syscalls)\n");
    kprintf("              - Enhanced admin subsystem enabled\n");
    kprintf("              - DisplayJet syscalls enabled\n");
}

int64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, 
                        uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    syscall_count++;
    
    process_t *proc = proc_current();
    pid_t pid = proc ? proc->pid : 0;
    
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
        case SYS_ADMIN:
            return sys_admin((int)arg1);
            
        /* DisplayJet syscalls */
        case SYS_DISPLAYJET_CREATE_SESSION:
            return (int64_t)(uintptr_t)displayjet_create_session(pid);
            
        case SYS_DISPLAYJET_ALLOC_BUFFER:
            return displayjet_allocate_buffer(pid, (size_t)arg1, (size_t)arg2, 
                                             (uint32_t)arg3, (uint32_t)arg4);
            
        case SYS_DISPLAYJET_FREE_BUFFER:
            return displayjet_free_buffer(pid, (uint32_t)arg1);
            
        case SYS_DISPLAYJET_UPDATE_BUFFER:
            return displayjet_update_buffer(pid, (uint32_t)arg1, 
                                           (const void*)(uintptr_t)arg2, (size_t)arg3);
            
        case SYS_DISPLAYJET_REQUEST_ACCESS:
            return displayjet_request_access(pid, (pid_t)arg1, (uint32_t)arg2, 
                                            (const char*)(uintptr_t)arg3);
            
        case SYS_DISPLAYJET_VERIFY_KEY:
            return displayjet_verify_key((uint64_t)arg1, (const uint8_t*)(uintptr_t)arg2);
            
        case SYS_DISPLAYJET_DECRYPT_ONCE: {
            displayjet_access_key_t *key = (displayjet_access_key_t*)(uintptr_t)arg1;
            return (int64_t)(uintptr_t)displayjet_decrypt_buffer_once(key);
        }
            
        default:
            return -1;
    }
}
