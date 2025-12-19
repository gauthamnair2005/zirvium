#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int adreno_init(void) { kprintf("GPU: Qualcomm Adreno (300/400/500/600/700 series)\n"); return 0; }
int mali_init(void) { kprintf("GPU: ARM Mali (G31/G52/G57/G68/G77/G78/G610)\n"); return 0; }
int powervr_init(void) { kprintf("GPU: PowerVR Midas/Rogue/Series9XE\n"); return 0; }
int apple_gpu_init(void) { kprintf("GPU: Apple GPU (M1/M2/M3/M4)\n"); return 0; }
