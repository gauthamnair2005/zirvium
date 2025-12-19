#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int sb16_init(void) { kprintf("Audio: Sound Blaster 16\n"); return 0; }
int sb_live_init(void) { kprintf("Audio: Sound Blaster Live!\n"); return 0; }
int sb_audigy_init(void) { kprintf("Audio: Sound Blaster Audigy\n"); return 0; }
int sb_zxr_init(void) { kprintf("Audio: Sound Blaster ZxR\n"); return 0; }
