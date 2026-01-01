#ifndef KERNEL_ADMIN_H
#define KERNEL_ADMIN_H

#include <kernel/types.h>
#include <kernel/process.h>

/* Admin privilege levels */
#define ADMIN_LEVEL_USER        0  /* Normal user */
#define ADMIN_LEVEL_ELEVATED    1  /* Elevated privileges */
#define ADMIN_LEVEL_ADMIN       2  /* Full admin */
#define ADMIN_LEVEL_SYSTEM      3  /* System/kernel level */

/* Admin capability flags */
#define ADMIN_CAP_NONE          0x00000000
#define ADMIN_CAP_READ_SYSTEM   0x00000001  /* Read system files */
#define ADMIN_CAP_WRITE_SYSTEM  0x00000002  /* Write system files */
#define ADMIN_CAP_PROC_MANAGE   0x00000004  /* Manage processes */
#define ADMIN_CAP_MEM_MANAGE    0x00000008  /* Manage memory */
#define ADMIN_CAP_DEV_MANAGE    0x00000010  /* Manage devices */
#define ADMIN_CAP_NET_ADMIN     0x00000020  /* Network admin */
#define ADMIN_CAP_MOUNT_FS      0x00000040  /* Mount filesystems */
#define ADMIN_CAP_LOAD_MODULE   0x00000080  /* Load kernel modules */
#define ADMIN_CAP_REBOOT        0x00000100  /* Reboot system */
#define ADMIN_CAP_KILL_ANY      0x00000200  /* Kill any process */
#define ADMIN_CAP_CHOWN         0x00000400  /* Change ownership */
#define ADMIN_CAP_RAW_IO        0x00000800  /* Direct hardware I/O */

/* Full admin capabilities */
#define ADMIN_CAP_ALL           0xFFFFFFFF

/* Admin command codes */
#define ADMIN_CMD_ELEVATE       1  /* Request elevation */
#define ADMIN_CMD_DROP          2  /* Drop privileges */
#define ADMIN_CMD_CHECK         3  /* Check current level */
#define ADMIN_CMD_GRANT_CAP     4  /* Grant capability */
#define ADMIN_CMD_REVOKE_CAP    5  /* Revoke capability */
#define ADMIN_CMD_LIST_CAPS     6  /* List capabilities */

/* Admin session structure */
typedef struct admin_session {
    pid_t pid;
    uint32_t level;
    uint32_t capabilities;
    uint64_t timestamp;
    uint32_t flags;
    struct admin_session *next;
} admin_session_t;

/* Admin subsystem functions */
void admin_init(void);
int admin_elevate(pid_t pid, uint32_t level);
int admin_drop(pid_t pid);
int admin_check_capability(pid_t pid, uint32_t capability);
int admin_grant_capability(pid_t pid, uint32_t capability);
int admin_revoke_capability(pid_t pid, uint32_t capability);
uint32_t admin_get_level(pid_t pid);
uint32_t admin_get_capabilities(pid_t pid);
int admin_is_admin(pid_t pid);
int admin_is_system(pid_t pid);

/* Admin audit logging */
void admin_log_access(pid_t pid, const char *resource, int granted);
void admin_log_elevation(pid_t pid, uint32_t old_level, uint32_t new_level);

#endif
