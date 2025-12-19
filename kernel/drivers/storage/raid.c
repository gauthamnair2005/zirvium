#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int raid0_init(void) { kprintf("RAID: RAID 0 (striping) support\n"); return 0; }
int raid1_init(void) { kprintf("RAID: RAID 1 (mirroring) support\n"); return 0; }
int raid5_init(void) { kprintf("RAID: RAID 5 (parity) support\n"); return 0; }
int raid6_init(void) { kprintf("RAID: RAID 6 (dual parity) support\n"); return 0; }
int raid10_init(void) { kprintf("RAID: RAID 10 (mirrored stripes) support\n"); return 0; }
