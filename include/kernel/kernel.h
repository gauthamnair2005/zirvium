#ifndef KERNEL_H
#define KERNEL_H

#include <kernel/types.h>

/* KERNEL_VERSION is defined by build system via -DKERNEL_VERSION */
#ifndef KERNEL_VERSION
#define KERNEL_VERSION "1.0.0"
#endif

#define KERNEL_NAME "Zirvium"
#define KERNEL_FULL_NAME "Zirvium Operating System"

/* Core kernel functions */
void kernel_panic(const char *msg);
void kprintf(const char *fmt, ...);

/* Build information */
#define BUILD_SYSTEM "Zirvium Build System v2.0"
#define SUPPORTED_DRIVERS 12787

#endif
