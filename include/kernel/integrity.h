#ifndef KERNEL_INTEGRITY_H
#define KERNEL_INTEGRITY_H

#include <kernel/types.h>

/* Integrity check flags */
#define INTEGRITY_CHECK_MEMORY    0x01
#define INTEGRITY_CHECK_PROCESS   0x02
#define INTEGRITY_CHECK_VFS       0x04
#define INTEGRITY_CHECK_DRIVER    0x08
#define INTEGRITY_CHECK_ALL       0xFF

/* Integrity status */
#define INTEGRITY_OK              0
#define INTEGRITY_WARNING         1
#define INTEGRITY_CRITICAL        2

/* Integrity subsystem */
void integrity_init(void);
int integrity_check(uint32_t flags);
int integrity_verify_memory(void);
int integrity_verify_processes(void);
int integrity_verify_vfs(void);
void integrity_report(void);

/* Stack canary protection */
void stack_canary_init(void);
int stack_canary_check(void);

/* Kernel address space protection */
int kernel_addr_is_valid(void *addr);
int userspace_addr_is_valid(void *addr, size_t size);

#endif
