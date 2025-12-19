/*
 * Zirvium Kernel Logging
 * printk and kernel message system
 */

#ifndef _KERNEL_PRINTK_H
#define _KERNEL_PRINTK_H

#include <kernel/types.h>
#include <stdarg.h>

/* Log levels */
#define KERN_EMERG   "<0>"  /* System is unusable */
#define KERN_ALERT   "<1>"  /* Action must be taken immediately */
#define KERN_CRIT    "<2>"  /* Critical conditions */
#define KERN_ERR     "<3>"  /* Error conditions */
#define KERN_WARNING "<4>"  /* Warning conditions */
#define KERN_NOTICE  "<5>"  /* Normal but significant */
#define KERN_INFO    "<6>"  /* Informational */
#define KERN_DEBUG   "<7>"  /* Debug-level messages */

/* Main kernel print function */
int printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int vprintk(const char *fmt, va_list args);

/* Console output functions */
void console_write(const char *str, size_t len);
void console_putchar(char c);

/* Early boot printing (before console is initialized) */
int early_printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Log buffer functions */
void log_store(int level, const char *msg);
void log_dump(void);

/* Conditional printing macros */
#ifdef DEBUG
#define pr_debug(fmt, ...) printk(KERN_DEBUG fmt, ##__VA_ARGS__)
#else
#define pr_debug(fmt, ...) do {} while (0)
#endif

#define pr_emerg(fmt, ...) printk(KERN_EMERG fmt, ##__VA_ARGS__)
#define pr_alert(fmt, ...) printk(KERN_ALERT fmt, ##__VA_ARGS__)
#define pr_crit(fmt, ...) printk(KERN_CRIT fmt, ##__VA_ARGS__)
#define pr_err(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...) printk(KERN_WARNING fmt, ##__VA_ARGS__)
#define pr_warn pr_warning
#define pr_notice(fmt, ...) printk(KERN_NOTICE fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...) printk(KERN_INFO fmt, ##__VA_ARGS__)

/* Device printing (when driver framework is available) */
#define dev_printk(level, dev, fmt, ...) \
    printk(level "%s: " fmt, (dev)->name, ##__VA_ARGS__)

#define dev_emerg(dev, fmt, ...) dev_printk(KERN_EMERG, dev, fmt, ##__VA_ARGS__)
#define dev_alert(dev, fmt, ...) dev_printk(KERN_ALERT, dev, fmt, ##__VA_ARGS__)
#define dev_crit(dev, fmt, ...) dev_printk(KERN_CRIT, dev, fmt, ##__VA_ARGS__)
#define dev_err(dev, fmt, ...) dev_printk(KERN_ERR, dev, fmt, ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...) dev_printk(KERN_WARNING, dev, fmt, ##__VA_ARGS__)
#define dev_notice(dev, fmt, ...) dev_printk(KERN_NOTICE, dev, fmt, ##__VA_ARGS__)
#define dev_info(dev, fmt, ...) dev_printk(KERN_INFO, dev, fmt, ##__VA_ARGS__)

#ifdef DEBUG
#define dev_dbg(dev, fmt, ...) dev_printk(KERN_DEBUG, dev, fmt, ##__VA_ARGS__)
#else
#define dev_dbg(dev, fmt, ...) do {} while (0)
#endif

#endif /* _KERNEL_PRINTK_H */
