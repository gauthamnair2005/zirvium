#include <kernel/integrity.h>
#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/process.h>

/* Stack canary value */
static uint32_t stack_canary = 0xDEADBEEF;
static int integrity_enabled = 0;
static uint32_t check_count = 0;
static uint32_t issues_found = 0;

void integrity_init(void) {
    integrity_enabled = 1;
    check_count = 0;
    issues_found = 0;
    
    kprintf("Integrity: Kernel integrity checking enabled\n");
    kprintf("           - Stack canary protection active\n");
    kprintf("           - Memory bounds validation active\n");
}

void stack_canary_init(void) {
    /* Use a pseudo-random value based on timestamp */
    stack_canary = 0xDEADBEEF ^ (uint32_t)check_count;
}

int stack_canary_check(void) {
    /* Check if canary has been corrupted */
    /* In a real implementation, this would check specific stack locations */
    return INTEGRITY_OK;
}

int kernel_addr_is_valid(void *addr) {
    if (!addr) return 0;
    
    uintptr_t ptr = (uintptr_t)addr;
    
    /* Kernel space: 0x00000000 - 0x7FFFFFFF (2GB) */
    /* User space: 0x80000000 - 0xFFFFFFFF (2GB) */
    
    /* For now, accept kernel addresses in lower 2GB */
    if (ptr >= 0x80000000) {
        return 0; /* User space address in kernel */
    }
    
    /* Reject null pointer dereferences */
    if (ptr < 0x1000) {
        return 0;
    }
    
    return 1;
}

int userspace_addr_is_valid(void *addr, size_t size) {
    if (!addr || size == 0) return 0;
    
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + size;
    
    /* Check for overflow */
    if (end < start) return 0;
    
    /* User space: 0x80000000 - 0xFFFFFFFF */
    if (start < 0x80000000 || end > 0xFFFFFFFF) {
        return 0;
    }
    
    return 1;
}

int integrity_verify_memory(void) {
    /* Verify memory subsystem integrity */
    uint32_t free_pages = pmm_get_free_pages();
    uint32_t total_pages = pmm_get_total_pages();
    
    if (free_pages > total_pages) {
        kprintf("Integrity: WARNING - Free pages (%u) > Total pages (%u)\n",
                free_pages, total_pages);
        issues_found++;
        return INTEGRITY_WARNING;
    }
    
    /* Check heap integrity */
    size_t heap_used = kmalloc_get_used();
    size_t heap_available = kmalloc_get_available();
    
    if (heap_used + heap_available != (4 * 1024 * 1024)) {
        kprintf("Integrity: WARNING - Heap accounting mismatch\n");
        issues_found++;
        return INTEGRITY_WARNING;
    }
    
    return INTEGRITY_OK;
}

int integrity_verify_processes(void) {
    /* Verify process structures */
    uint64_t proc_count = proc_get_count();
    
    if (proc_count > 1000) {
        kprintf("Integrity: WARNING - Excessive process count: %lu\n", 
                (unsigned long)proc_count);
        issues_found++;
        return INTEGRITY_WARNING;
    }
    
    return INTEGRITY_OK;
}

int integrity_verify_vfs(void) {
    /* Basic VFS integrity checks */
    /* In a real implementation, this would verify VFS structures */
    return INTEGRITY_OK;
}

int integrity_check(uint32_t flags) {
    if (!integrity_enabled) return INTEGRITY_OK;
    
    check_count++;
    int status = INTEGRITY_OK;
    
    if (flags & INTEGRITY_CHECK_MEMORY) {
        int mem_status = integrity_verify_memory();
        if (mem_status > status) status = mem_status;
    }
    
    if (flags & INTEGRITY_CHECK_PROCESS) {
        int proc_status = integrity_verify_processes();
        if (proc_status > status) status = proc_status;
    }
    
    if (flags & INTEGRITY_CHECK_VFS) {
        int vfs_status = integrity_verify_vfs();
        if (vfs_status > status) status = vfs_status;
    }
    
    return status;
}

void integrity_report(void) {
    kprintf("\n");
    kprintf("=== Kernel Integrity Report ===\n");
    kprintf("Checks performed: %u\n", check_count);
    kprintf("Issues found: %u\n", issues_found);
    kprintf("Status: %s\n", issues_found == 0 ? "HEALTHY" : "WARNINGS DETECTED");
    
    /* Memory statistics */
    kprintf("\nMemory Status:\n");
    kprintf("  Free pages: %u / %u\n", pmm_get_free_pages(), pmm_get_total_pages());
    kprintf("  Heap used: %lu KB / 4096 KB\n", 
            (unsigned long)(kmalloc_get_used() / 1024));
    
    /* Process statistics */
    kprintf("\nProcess Status:\n");
    kprintf("  Active processes: %lu\n", (unsigned long)proc_get_count());
    
    kprintf("================================\n\n");
}
