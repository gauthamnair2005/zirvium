/* Zirv Subsystem Device Manager */
#include <kernel/mosix.h>
#include <kernel/zirvfs.h>
#include <kernel/kernel.h>

static void register_path(const char *path, const char *desc) {
    kprintf("Zirv: Registered %s [%s]\n", path, desc);
}

void mosix_init(void) {
    kprintf("MOSIX: Initializing /zirv subsystem...\n");
    
    register_path(MOSIX_ZIRV_SATA, "SATA Controller");
    register_path(MOSIX_ZIRV_NVME, "NVMe Controller");
    register_path(MOSIX_ZIRV_USB, "Universal Serial Bus");
    
    // Control Nodes
    register_path("/zirv/mkfs", "ZirvFS Kernel Formatter (Write device path to trigger)");
}

/*
 * Simulate a write to the control node
 * This effectively acts as the ioctl/syscall handler for formatting
 */
void zirv_control_write(const char *node, const char *data) {
    // In a real VFS, this would be vfs_write -> node->write
    
    // Check if writing to mkfs node
    int i = 0;
    while (node[i] && node[i] == "/zirv/mkfs"[i]) i++;
    
    if (i >= 10) { // Match
        kprintf("ZirvControl: Received Format Request for '%s'\n", data);
        zirvfs_format(data, "New Volume");
    }
}

int mosix_register_device(const char *name, zirv_dev_type_t type, zirv_dev_subtype_t subtype) {
    (void)type;
    (void)subtype;
    // In real impl, this would create a node in the VFS tree under /zirv
    kprintf("Zirv: New Device Discovered: %s\n", name);
    return 0;
}
