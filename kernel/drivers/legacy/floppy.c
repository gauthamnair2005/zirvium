#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int floppy_init(void) { kprintf("Floppy: 3.5\" and 5.25\" drive support\n"); return 0; }
int zipdrive_init(void) { kprintf("ZIP: Iomega ZIP drive (100MB/250MB/750MB)\n"); return 0; }
int ide_init(void) { kprintf("IDE/PATA: Legacy parallel ATA support\n"); return 0; }
