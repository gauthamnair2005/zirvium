#include <kernel/kernel.h>
#include <arch/x64.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static uint16_t *vga_buffer = (uint16_t*)VGA_MEMORY;
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t color = 0x07;

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
