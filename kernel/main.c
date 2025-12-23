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
extern void zirvfs_init(void);
extern void mosix_init(void);

static void print_banner(void) {
    console_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    kprintf("\n");
    kprintf("  _______ _______      _______ _     _ _____ _     _ _______ \n");
    kprintf(" |_____|   |   |____| |______  \\___/    |   |     | |  |  | \n");
    kprintf(" |     | __|__ |    |  ______| _/ \\_  __|__ |_____| |  |  | \n");
    kprintf("\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    kprintf("        Zirvium Operating System v%s\n", KERNEL_VERSION);
    console_set_color(COLOR_LIGHT_GREY, COLOR_BLACK);
    kprintf("        Universal Multi-Architecture Kernel\n");
    kprintf("        Supporting 35,000+ Devices\n");
    console_reset_color();
    kprintf("\n");
    boot_separator();
    kprintf("\n");
}

static int init_subsystem(const char *name, void (*init_func)(void)) {
    boot_msg_starting(name);
    init_func();
    boot_msg_ok(name);
    return 0;
}

void kernel_main(uint32_t magic __attribute__((unused)), uint32_t addr __attribute__((unused))) {
extern void serial_8250_init(void);

    // Initial direct VGA write
    volatile uint16_t *vga = (uint16_t*)0xB8000;
    vga[1] = 0x2A4B;  // 'K' in green on black
    
    // Initialize serial first for logging
    serial_8250_init();
    
    console_init();
    print_banner();
    
    boot_header("Kernel Initialization");
    boot_msg_info("Starting kernel initialization sequence...");
    kprintf("\n");
    
    // Detect architecture
    boot_msg_info("Detecting system architecture...");
    arch_print_info();
    
    // Detect CPU features
    cpu_feature_init();
    cpu_print_info();
    
    // Check for DIY/Homebrew CPUs
    #ifdef CONFIG_ZELOOF_Z2
    zeloof_init();
    #endif
    
    kprintf("\n");
    boot_header("Core Subsystems");
    
    // Core architecture setup
    init_subsystem("Global Descriptor Table (GDT)", gdt_init);
    init_subsystem("Interrupt Descriptor Table (IDT)", idt_init);
    
    kprintf("\n");
    boot_header("Memory Management");
    
    // Memory management
    init_subsystem("Physical Memory Manager (PMM)", pmm_init);
    init_subsystem("Virtual Memory Manager (VMM)", vmm_init);
    init_subsystem("Kernel Heap Allocator", kmalloc_init);
    
    kprintf("\n");
    boot_header("File Systems & I/O");
    
    // Virtual File System
    init_subsystem("Virtual File System (VFS)", vfs_init);
    init_subsystem("ZirvFS Core", zirvfs_init);
    init_subsystem("MOSIX Subsystem", mosix_init);
    
    // Simulate User formatting a drive via /zirv/mkfs
    // In a real scenario, this happens from userspace
    kprintf("\n[AUTO-TEST] Triggering ZirvFS Format via /zirv/mkfs...\n");
    extern void zirv_control_write(const char *node, const char *data);
    zirv_control_write("/zirv/mkfs", "/zirv/nvme/disk0");
    
    // System calls
    init_subsystem("System Call Interface", syscall_init);
    
    kprintf("\n");
    boot_header("Process Management");
    
    // Process management
    init_subsystem("Process Scheduler", proc_init);
    
    kprintf("\n");
    boot_header("Device Drivers");
    
    // Driver subsystem
    init_subsystem("Driver Framework", driver_subsystem_init);
    
    // Enable interrupts
    interrupts_enable();
    boot_msg_ok("Hardware Interrupts");
    
    kprintf("\n");
    boot_separator();
    console_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    kprintf("\n    Kernel initialization complete!\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    kprintf("    System ready. Entering idle loop...\n");
    console_reset_color();
    boot_separator();
    kprintf("\n");
    
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
