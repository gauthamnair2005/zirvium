#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/process.h>
#include <kernel/syscall.h>
#include <kernel/vfs.h>
#include <arch/x64.h>

extern void console_init(void);

void kernel_main(void) {
    console_init();
    kprintf("Zirvium OS v%s\n", KERNEL_VERSION);
    kprintf("Initializing kernel...\n");
    
    gdt_init();
    kprintf("[OK] GDT initialized\n");
    
    idt_init();
    kprintf("[OK] IDT initialized\n");
    
    mm_init();
    kprintf("[OK] Memory management initialized\n");
    
    vfs_init();
    kprintf("[OK] Virtual file system initialized\n");
    
    syscall_init();
    kprintf("[OK] System calls initialized\n");
    
    proc_init();
    kprintf("[OK] Process management initialized\n");
    
    interrupts_enable();
    kprintf("[OK] Interrupts enabled\n");
    
    kprintf("\nKernel initialization complete!\n");
    kprintf("Welcome to Zirvium OS\n\n");
    
    for(;;) {
        asm volatile("hlt");
    }
}

void kernel_panic(const char *msg) {
    interrupts_disable();
    kprintf("\n*** KERNEL PANIC ***\n");
    kprintf("%s\n", msg);
    for(;;) {
        asm volatile("hlt");
    }
}
