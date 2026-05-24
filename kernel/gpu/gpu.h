/* kernel/gpu/gpu.h
 * Zirvium Kernel — GPU subsystem
 */
#ifndef ZIRVIUM_KERNEL_GPU_GPU_H
#define ZIRVIUM_KERNEL_GPU_GPU_H

#include "drivers/zirv/device.h"
#include <stdint.h>
#include <stddef.h>

/**
 * gpu_init - Initialize the GPU subsystem.
 */
void gpu_init(void);

/**
 * gpu_submit - Submit a command buffer to the primary GPU.
 */
int gpu_submit(const void *buf, size_t size);

#endif /* ZIRVIUM_KERNEL_GPU_GPU_H */
