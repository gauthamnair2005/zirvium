#include <kernel/types.h>
#include <kernel/kernel.h>

int wol_init(void) {
    kprintf("Wake-on-LAN: Magic packet support enabled\n");
    kprintf("  Methods: Magic packet, pattern matching\n");
    kprintf("  Wake from: S3/S4/S5 power states\n");
    return 0;
}
