#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int uvc_init(void) { kprintf("Webcam: USB Video Class (UVC)\n"); return 0; }
int webcam_logitech_init(void) { kprintf("Webcam: Logitech C920/C922/Brio\n"); return 0; }
int webcam_microsoft_init(void) { kprintf("Webcam: Microsoft LifeCam\n"); return 0; }
