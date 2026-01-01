#include <kernel/admin.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/process.h>

static admin_session_t *admin_sessions = NULL;
static uint32_t admin_count = 0;
static uint64_t admin_timestamp = 0;

/* Initialize admin subsystem */
void admin_init(void) {
    admin_sessions = NULL;
    admin_count = 0;
    admin_timestamp = 0;
    
    kprintf("Admin: Kernel-level superuser system initialized\n");
    kprintf("       - Boot process (PID 0) granted system level\n");
    
    /* Grant system level to boot process */
    admin_elevate(0, ADMIN_LEVEL_SYSTEM);
}

/* Find admin session for PID */
static admin_session_t *admin_find_session(pid_t pid) {
    admin_session_t *session = admin_sessions;
    while (session) {
        if (session->pid == pid) {
            return session;
        }
        session = session->next;
    }
    return NULL;
}

/* Create new admin session */
static admin_session_t *admin_create_session(pid_t pid) {
    admin_session_t *session = (admin_session_t*)kmalloc(sizeof(admin_session_t));
    if (!session) {
        kprintf("Admin: Failed to allocate session for PID %d\n", pid);
        return NULL;
    }
    
    session->pid = pid;
    session->level = ADMIN_LEVEL_USER;
    session->capabilities = ADMIN_CAP_NONE;
    session->timestamp = admin_timestamp++;
    session->flags = 0;
    session->next = admin_sessions;
    admin_sessions = session;
    admin_count++;
    
    return session;
}

/* Elevate process to admin level */
int admin_elevate(pid_t pid, uint32_t level) {
    if (level > ADMIN_LEVEL_SYSTEM) {
        return -1;
    }
    
    admin_session_t *session = admin_find_session(pid);
    if (!session) {
        session = admin_create_session(pid);
        if (!session) return -1;
    }
    
    uint32_t old_level = session->level;
    session->level = level;
    
    /* Grant capabilities based on level */
    switch (level) {
        case ADMIN_LEVEL_USER:
            session->capabilities = ADMIN_CAP_NONE;
            break;
        case ADMIN_LEVEL_ELEVATED:
            session->capabilities = ADMIN_CAP_READ_SYSTEM | 
                                   ADMIN_CAP_PROC_MANAGE;
            break;
        case ADMIN_LEVEL_ADMIN:
            session->capabilities = ADMIN_CAP_READ_SYSTEM | 
                                   ADMIN_CAP_WRITE_SYSTEM |
                                   ADMIN_CAP_PROC_MANAGE |
                                   ADMIN_CAP_MEM_MANAGE |
                                   ADMIN_CAP_DEV_MANAGE |
                                   ADMIN_CAP_NET_ADMIN |
                                   ADMIN_CAP_MOUNT_FS |
                                   ADMIN_CAP_KILL_ANY |
                                   ADMIN_CAP_CHOWN;
            break;
        case ADMIN_LEVEL_SYSTEM:
            session->capabilities = ADMIN_CAP_ALL;
            break;
    }
    
    /* Update process permissions */
    process_t *proc = proc_current();
    if (proc && proc->pid == pid) {
        if (level >= ADMIN_LEVEL_ADMIN) {
            proc->permissions |= PROC_PERM_ROOT;
        } else {
            proc->permissions &= ~PROC_PERM_ROOT;
        }
    }
    
    admin_log_elevation(pid, old_level, level);
    return 0;
}

/* Drop admin privileges */
int admin_drop(pid_t pid) {
    admin_session_t *session = admin_find_session(pid);
    if (!session) return -1;
    
    uint32_t old_level = session->level;
    session->level = ADMIN_LEVEL_USER;
    session->capabilities = ADMIN_CAP_NONE;
    
    /* Update process permissions */
    process_t *proc = proc_current();
    if (proc && proc->pid == pid) {
        proc->permissions &= ~PROC_PERM_ROOT;
    }
    
    admin_log_elevation(pid, old_level, ADMIN_LEVEL_USER);
    return 0;
}

/* Check if process has capability */
int admin_check_capability(pid_t pid, uint32_t capability) {
    admin_session_t *session = admin_find_session(pid);
    if (!session) return 0;
    
    return (session->capabilities & capability) != 0;
}

/* Grant specific capability */
int admin_grant_capability(pid_t pid, uint32_t capability) {
    /* Only system level can grant capabilities */
    process_t *current = proc_current();
    if (!current || !admin_is_system(current->pid)) {
        return -1;
    }
    
    admin_session_t *session = admin_find_session(pid);
    if (!session) {
        session = admin_create_session(pid);
        if (!session) return -1;
    }
    
    session->capabilities |= capability;
    kprintf("Admin: Granted capability 0x%x to PID %d\n", capability, pid);
    return 0;
}

/* Revoke specific capability */
int admin_revoke_capability(pid_t pid, uint32_t capability) {
    /* Only system level can revoke capabilities */
    process_t *current = proc_current();
    if (!current || !admin_is_system(current->pid)) {
        return -1;
    }
    
    admin_session_t *session = admin_find_session(pid);
    if (!session) return -1;
    
    session->capabilities &= ~capability;
    kprintf("Admin: Revoked capability 0x%x from PID %d\n", capability, pid);
    return 0;
}

/* Get admin level */
uint32_t admin_get_level(pid_t pid) {
    admin_session_t *session = admin_find_session(pid);
    return session ? session->level : ADMIN_LEVEL_USER;
}

/* Get capabilities */
uint32_t admin_get_capabilities(pid_t pid) {
    admin_session_t *session = admin_find_session(pid);
    return session ? session->capabilities : ADMIN_CAP_NONE;
}

/* Check if admin */
int admin_is_admin(pid_t pid) {
    admin_session_t *session = admin_find_session(pid);
    return session && session->level >= ADMIN_LEVEL_ADMIN;
}

/* Check if system level */
int admin_is_system(pid_t pid) {
    admin_session_t *session = admin_find_session(pid);
    return session && session->level >= ADMIN_LEVEL_SYSTEM;
}

/* Log access attempt */
void admin_log_access(pid_t pid, const char *resource, int granted) {
    if (!resource) return;
    
    admin_session_t *session = admin_find_session(pid);
    const char *level_str = "USER";
    
    if (session) {
        switch (session->level) {
            case ADMIN_LEVEL_ELEVATED: level_str = "ELEVATED"; break;
            case ADMIN_LEVEL_ADMIN: level_str = "ADMIN"; break;
            case ADMIN_LEVEL_SYSTEM: level_str = "SYSTEM"; break;
        }
    }
    
    kprintf("Admin: [%s] PID %d %s access to '%s'\n", 
            level_str, pid, granted ? "granted" : "DENIED", resource);
}

/* Log privilege elevation */
void admin_log_elevation(pid_t pid, uint32_t old_level, uint32_t new_level) {
    const char *old_str = "USER";
    const char *new_str = "USER";
    
    switch (old_level) {
        case ADMIN_LEVEL_ELEVATED: old_str = "ELEVATED"; break;
        case ADMIN_LEVEL_ADMIN: old_str = "ADMIN"; break;
        case ADMIN_LEVEL_SYSTEM: old_str = "SYSTEM"; break;
    }
    
    switch (new_level) {
        case ADMIN_LEVEL_ELEVATED: new_str = "ELEVATED"; break;
        case ADMIN_LEVEL_ADMIN: new_str = "ADMIN"; break;
        case ADMIN_LEVEL_SYSTEM: new_str = "SYSTEM"; break;
    }
    
    if (old_level != new_level) {
        kprintf("Admin: PID %d privilege change: %s -> %s\n", pid, old_str, new_str);
    }
}
