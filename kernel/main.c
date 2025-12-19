#include <kernel/kernel.h>
#include <kernel/memory.h>
#include <kernel/process.h>
#include <kernel/syscall.h>
#include <kernel/vfs.h>
#include <kernel/driver.h>
#include <arch/x64.h>
#include <kernel/types.h>

extern void console_init(void);
extern void driver_subsystem_init(void);
extern void arch_print_info(void);
extern void cpu_feature_init(void);
extern void cpu_print_info(void);
extern int zeloof_init(void);

static void print_banner(void) {
    kprintf("\n");
    kprintf("  _______ _______      _______ _     _ _____ _     _ _______ \n");
    kprintf(" |_____|   |   |____| |______  \\___/    |   |     | |  |  | \n");
    kprintf(" |     | __|__ |    |  ______| _/ \\_  __|__ |_____| |  |  | \n");
    kprintf("\n");
    kprintf("        Zirvium Operating System v%s\n", KERNEL_VERSION);
    kprintf("        Universal Multi-Architecture Kernel\n");
    kprintf("        Supporting 35,000+ Devices\n");
    kprintf("\n");
}

static int init_subsystem(const char *name, void (*init_func)(void)) {
    kprintf("[ .. ] Initializing %s...\r", name);
    init_func();
    kprintf("[ OK ] %s initialized\n", name);
    return 0;
}

void kernel_main(uint32_t magic __attribute__((unused)), uint32_t addr __attribute__((unused))) {
    // Direct VGA write to prove we got here - write "K" in green
    volatile uint16_t *vga = (uint16_t*)0xB8000;
    vga[1] = 0x2A4B;  // 'K' in green on black
    
    console_init();
    print_banner();
    
    kprintf("Starting kernel initialization sequence...\n\n");
    
    // Detect architecture
    kprintf("Detecting system architecture...\n");
    arch_print_info();
    
    // Detect CPU features
    cpu_feature_init();
    cpu_print_info();
    
    // Check for DIY/Homebrew CPUs
    #ifdef CONFIG_ZELOOF_Z2
    zeloof_init();
    #endif
    
    // Core architecture setup
    init_subsystem("GDT (Global Descriptor Table)", gdt_init);
    init_subsystem("IDT (Interrupt Descriptor Table)", idt_init);
    
    // Memory management
    init_subsystem("Memory Management (PMM/VMM)", mm_init);
    
    // Virtual File System
    init_subsystem("Virtual File System (VFS)", vfs_init);
    
    // System calls
    init_subsystem("System Call Interface", syscall_init);
    
    // Process management
    init_subsystem("Process Scheduler", proc_init);
    
    // Driver subsystem
    init_subsystem("Driver Framework", driver_subsystem_init);
    
    // Enable interrupts
    interrupts_enable();
    kprintf("[ OK ] Hardware interrupts enabled\n");
    
    kprintf("\n==============================================\n");
    kprintf("Kernel initialization complete!\n");
    kprintf("System ready. Entering idle loop...\n");
    kprintf("==============================================\n\n");
    
    // Idle loop with power management
    while(1) {
        asm volatile("hlt");
    }
}

void kernel_panic(const char *msg) {
    interrupts_disable();
    kprintf("\n");
    kprintf("===============================================\n");
    kprintf("***         KERNEL PANIC - ZIRVIUM         ***\n");
    kprintf("===============================================\n");
    kprintf("\nFatal Error: %s\n\n", msg);
    kprintf("The system has encountered a critical error\n");
    kprintf("and must halt to prevent data corruption.\n");
    kprintf("\nSystem halted.\n");
    kprintf("===============================================\n");
    
    // Halt all CPUs
    while(1) {
        asm volatile("cli; hlt");
    }
}
