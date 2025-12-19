#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int bios_legacy_init(void) { kprintf("Firmware: Legacy BIOS interface\n"); return 0; }
int uefi_init(void) { kprintf("Firmware: UEFI runtime services\n"); return 0; }
int uefi_gop_init(void) { kprintf("Firmware: UEFI Graphics Output Protocol\n"); return 0; }
int smbios_init(void) { kprintf("Firmware: SMBIOS/DMI tables\n"); return 0; }
