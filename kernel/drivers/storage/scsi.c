#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int scsi_init(void) { kprintf("SCSI: Small Computer System Interface\n"); return 0; }
int sas_init(void) { kprintf("SAS: Serial Attached SCSI\n"); return 0; }
