#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int android_binder_init(void) { kprintf("Android: Binder IPC\n"); return 0; }
int android_ashmem_init(void) { kprintf("Android: Anonymous shared memory\n"); return 0; }
int android_lowmemorykiller_init(void) { kprintf("Android: Low memory killer\n"); return 0; }
int android_logger_init(void) { kprintf("Android: Logger subsystem\n"); return 0; }
