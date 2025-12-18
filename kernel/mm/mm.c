#include <kernel/memory.h>
#include <kernel/kernel.h>

extern void pmm_init(void);
extern void vmm_init(void);

void mm_init(void) {
    pmm_init();
    vmm_init();
}
