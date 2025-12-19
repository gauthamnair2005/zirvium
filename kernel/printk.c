/*
 * Zirvium Kernel Logging Implementation
 */

#include <kernel/types.h>
#include <kernel/printk.h>
#include <stdarg.h>

/* External console functions */
extern void console_putchar(char c);
extern void console_write(const char *str, size_t len);

/* Simple number to string conversion */
static void print_number(long num, int base, int sign) {
    char buf[32];
    int i = 0;
    int neg = 0;
    
    if (sign && num < 0) {
        neg = 1;
        num = -num;
    }
    
    if (num == 0) {
        console_putchar('0');
        return;
    }
    
    while (num) {
        int digit = num % base;
        buf[i++] = (digit < 10) ? '0' + digit : 'a' + digit - 10;
        num /= base;
    }
    
    if (neg) {
        console_putchar('-');
    }
    
    while (i > 0) {
        console_putchar(buf[--i]);
    }
}

int vprintk(const char *fmt, va_list args) {
    const char *p = fmt;
    int count = 0;
    
    /* Skip log level prefix like "<6>" */
    if (fmt[0] == '<' && fmt[1] >= '0' && fmt[1] <= '7' && fmt[2] == '>') {
        p = fmt + 3;
    }
    
    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'd':
                case 'i':
                    print_number(va_arg(args, int), 10, 1);
                    break;
                case 'u':
                    print_number(va_arg(args, unsigned int), 10, 0);
                    break;
                case 'x':
                    print_number(va_arg(args, unsigned int), 16, 0);
                    break;
                case 'p':
                    console_putchar('0');
                    console_putchar('x');
                    print_number((unsigned long)va_arg(args, void *), 16, 0);
                    break;
                case 's': {
                    const char *s = va_arg(args, const char *);
                    if (s) {
                        while (*s) {
                            console_putchar(*s++);
                            count++;
                        }
                    } else {
                        const char *null_str = "(null)";
                        while (*null_str) {
                            console_putchar(*null_str++);
                            count++;
                        }
                    }
                    break;
                }
                case 'c':
                    console_putchar((char)va_arg(args, int));
                    count++;
                    break;
                case '%':
                    console_putchar('%');
                    count++;
                    break;
                default:
                    console_putchar('%');
                    console_putchar(*p);
                    count += 2;
                    break;
            }
            p++;
        } else {
            console_putchar(*p);
            count++;
            p++;
        }
    }
    
    return count;
}

int printk(const char *fmt, ...) {
    va_list args;
    int ret;
    
    va_start(args, fmt);
    ret = vprintk(fmt, args);
    va_end(args);
    
    return ret;
}

int early_printk(const char *fmt, ...) {
    va_list args;
    int ret;
    
    va_start(args, fmt);
    ret = vprintk(fmt, args);
    va_end(args);
    
    return ret;
}
