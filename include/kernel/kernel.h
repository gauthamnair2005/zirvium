#ifndef KERNEL_H
#define KERNEL_H

#include <kernel/types.h>

#define KERNEL_VERSION "0.1.0"

void kernel_panic(const char *msg);
void kprintf(const char *fmt, ...);

#endif
