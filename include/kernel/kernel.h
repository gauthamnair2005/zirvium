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

/* Console color functions */
void console_set_color(uint8_t fg, uint8_t bg);
void console_reset_color(void);
void console_save_color(void);

/* Systemd-style boot messages */
void boot_msg_starting(const char *service);
void boot_msg_ok(const char *service);
void boot_msg_fail(const char *service, const char *reason);
void boot_msg_info(const char *msg);
void boot_msg_warn(const char *msg);
void boot_msg_skip(const char *service);
void boot_separator(void);
void boot_header(const char *text);

/* VGA Color codes */
#define COLOR_BLACK         0x0
#define COLOR_BLUE          0x1
#define COLOR_GREEN         0x2
#define COLOR_CYAN          0x3
#define COLOR_RED           0x4
#define COLOR_MAGENTA       0x5
#define COLOR_BROWN         0x6
#define COLOR_LIGHT_GREY    0x7
#define COLOR_DARK_GREY     0x8
#define COLOR_LIGHT_BLUE    0x9
#define COLOR_LIGHT_GREEN   0xA
#define COLOR_LIGHT_CYAN    0xB
#define COLOR_LIGHT_RED     0xC
#define COLOR_LIGHT_MAGENTA 0xD
#define COLOR_YELLOW        0xE
#define COLOR_WHITE         0xF

/* Build information */
#define BUILD_SYSTEM "Zirvium Build System v2.0"
#define SUPPORTED_DRIVERS 12787

#endif
