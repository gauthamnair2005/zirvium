/* kernel/gpu/gpu.c
 * Zirvium Kernel — GPU subsystem implementation
 */
#include "gpu.h"
#include "gem.h"
#include "kernel/console.h"

static registered_device_t *g_gpu_dev = NULL;

void gpu_init(void)
{
    gem_init();

    /* Find the first GPU device */
    g_gpu_dev = zirv_find_device(DEV_CLASS_DISPLAY_GPU, DEV_CLASS_DISPLAY_FB, 0);
    if (g_gpu_dev) {
        klog(LOG_OK, "gpu", "Found primary GPU: %s", g_gpu_dev->desc.model);
    } else {
        klog(LOG_INFO, "gpu", "No hardware GPU found, using software fallback");
    }
}

int gpu_submit(const void *buf, size_t size)
{
    if (!g_gpu_dev) return -38; /* ENOSYS */
    if (!g_gpu_dev->ops->submit_command) return -38;

    return g_gpu_dev->ops->submit_command(&g_gpu_dev->desc, buf, size);
}
