#include <kernel/kernel.h>
#include <arch/x64.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// VGA Color codes
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

#define MAKE_COLOR(fg, bg) ((bg << 4) | fg)

static uint16_t *vga_buffer = (uint16_t*)((uintptr_t)VGA_MEMORY);
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t color = MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK);
static uint8_t saved_color = MAKE_COLOR(COLOR_LIGHT_GREY, COLOR_BLACK);

void console_set_color(uint8_t fg, uint8_t bg) {
    color = MAKE_COLOR(fg, bg);
}

void console_reset_color(void) {
    color = saved_color;
}

void console_save_color(void) {
    saved_color = color;
}

void console_init(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (color << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
}

static void scroll(void) {
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (color << 8) | ' ';
    }
    cursor_y = VGA_HEIGHT - 1;
}

void console_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            scroll();
        }
        return;
    }
    
    vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (color << 8) | c;
    cursor_x++;
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT) {
            scroll();
        }
    }
}

void console_write(const char *str) {
    while (*str) {
        console_putchar(*str++);
    }
}

static void print_num(uint64_t num, int base) {
    char buffer[32];
    int i = 0;
    
    if (num == 0) {
        console_putchar('0');
        return;
    }
    
    while (num > 0) {
        int digit = num % base;
        buffer[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        num /= base;
    }
    
    while (i > 0) {
        console_putchar(buffer[--i]);
    }
}

void kprintf(const char *fmt, ...) {
    uint64_t *args = (uint64_t*)&fmt + 1;
    int arg_index = 0;
    
    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *s = (const char*)args[arg_index++];
                    console_write(s);
                    break;
                }
                case 'd': {
                    int64_t n = (int64_t)args[arg_index++];
                    if (n < 0) {
                        console_putchar('-');
                        n = -n;
                    }
                    print_num(n, 10);
                    break;
                }
                case 'x': {
                    uint64_t n = args[arg_index++];
                    print_num(n, 16);
                    break;
                }
                case '%':
                    console_putchar('%');
                    break;
            }
        } else {
            console_putchar(*fmt);
        }
        fmt++;
    }
}

// Systemd-style boot status functions
void console_print_status(const char *msg, const char *status, uint8_t status_color) {
    // Save current position
    
    // Print message in normal color
    console_write(msg);
    
    // Move to right side of screen (column 65)
    cursor_x = 65;
    
    // Print status with color
    uint8_t old_color = color;
    color = MAKE_COLOR(status_color, COLOR_BLACK);
    console_write(status);
    color = old_color;
    
    // Move to next line
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= VGA_HEIGHT) {
        scroll();
    }
}

void boot_msg_starting(const char *service) {
    console_save_color();
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_write("[ .... ] Starting ");
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_write(service);
    console_reset_color();
    console_write("...\r");
}

void boot_msg_ok(const char *service) {
    console_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
    console_write("[  OK  ]");
    console_reset_color();
    console_write(" Started ");
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_write(service);
    console_reset_color();
    console_write("\n");
}

void boot_msg_fail(const char *service, const char *reason) {
    console_set_color(COLOR_LIGHT_RED, COLOR_BLACK);
    console_write("[ FAIL ]");
    console_reset_color();
    console_write(" Failed ");
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_write(service);
    console_reset_color();
    if (reason) {
        console_write(" - ");
        console_set_color(COLOR_YELLOW, COLOR_BLACK);
        console_write(reason);
        console_reset_color();
    }
    console_write("\n");
}

void boot_msg_info(const char *msg) {
    console_set_color(COLOR_LIGHT_BLUE, COLOR_BLACK);
    console_write("[ INFO ]");
    console_reset_color();
    console_write(" ");
    console_write(msg);
    console_write("\n");
}

void boot_msg_warn(const char *msg) {
    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_write("[ WARN ]");
    console_reset_color();
    console_write(" ");
    console_write(msg);
    console_write("\n");
}

void boot_msg_skip(const char *service) {
    console_set_color(COLOR_DARK_GREY, COLOR_BLACK);
    console_write("[ SKIP ]");
    console_reset_color();
    console_write(" Skipped ");
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_write(service);
    console_reset_color();
    console_write("\n");
}

void boot_separator(void) {
    console_set_color(COLOR_DARK_GREY, COLOR_BLACK);
    for (int i = 0; i < VGA_WIDTH; i++) {
        console_putchar('-');
    }
    console_reset_color();
}

void boot_header(const char *text) {
    console_write("\n");
    console_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    console_write("===> ");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_write(text);
    console_reset_color();
    console_write("\n");
    boot_separator();
}
