/* kernel/shell.c
 * Zirvium Kernel — ZirvShell (MOSIX English-like Shell)
 */
#include "shell.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/ctype.h"
#include "drivers/serial/serial.h"
#include "drivers/zirv/device.h"
#include "kernel/mm/pmm.h"
#include <stdint.h>
#include <stdbool.h>

#define SHELL_MAX_LINE 256

static void shell_prompt(void) {
    kprintf("root@Zirvium in / > ");
}

static void show_help(void) {
    kprintf("Available commands (MOSIX syntax):\n");
    kprintf("  help me please                       - Show this help message\n");
    kprintf("  show system status                   - Display kernel and memory information\n");
    kprintf("  list all devices                     - List all registered devices in /zirv\n");
    kprintf("  read sector <n> from <path>          - Display a hex dump of a device sector\n");
    kprintf("  clear the screen                     - Clear the terminal screen\n");
}

static void show_status(void) {
    kprintf("Zirvium Kernel v0.1.0\n");
    kprintf("MOSIX Compliant (Modern OSIX)\n");
    kprintf("Memory Status:\n");
    kprintf("  Total pages: %u\n", pmm_total_pages());
    kprintf("  Free pages:  %u\n", pmm_free_page_count());
}

static void list_devices(void) {
    kprintf("Registered devices in /zirv:\n");
    /* We need a way to iterate through devices. 
     * Let's check drivers/zirv/device.h for iteration. */
    // For now, I'll assume we can't easily iterate without modifying device.c
    // I'll add a helper to device.c later.
    kprintf("(Device iteration not yet implemented in VFS)\n");
}

static void read_sector(uint64_t sector, const char *path) {
    /* TODO: find device by path and call read_sectors */
    kprintf("Reading sector %u from %s...\n", (uint32_t)sector, path);
    // Find device
    registered_device_t *dev = zirv_find_device_by_path(path);
    if (!dev) {
        kprintf("Error: Device %s not found.\n", path);
        return;
    }

    uint8_t buf[512];
    int res = dev->ops->read_sectors(&dev->desc, sector, 1, buf);
    if (res < 0) {
        kprintf("Error: Failed to read sector (code %d).\n", res);
        return;
    }

    /* Hex dump */
    for (int i = 0; i < 512; i++) {
        if (i % 16 == 0) kprintf("\n%x: ", i);
        kprintf("%x ", buf[i]);
    }
    kprintf("\n");
}

static void shell_execute(char *line) {
    /* Trim trailing newline */
    size_t len = strlen(line);
    if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
    if (len > 1 && line[len-2] == '\r') line[len-2] = '\0';

    if (strcmp(line, "help me please") == 0) {
        show_help();
    } else if (strcmp(line, "show system status") == 0) {
        show_status();
    } else if (strcmp(line, "list all devices") == 0) {
        list_devices();
    } else if (strstr(line, "read sector ") == line) {
        /* Simple parser for "read sector <n> from <path>" */
        char *p = line + 12;
        char *end;
        uint64_t sector = 0;
        while (*p && isdigit(*p)) {
            sector = sector * 10 + (*p - '0');
            p++;
        }
        while (*p && isspace(*p)) p++;
        if (strstr(p, "from ") == p) {
            p += 5;
            while (*p && isspace(*p)) p++;
            read_sector(sector, p);
        } else {
            kprintf("Usage: read sector <n> from <device>\n");
        }
    } else if (strcmp(line, "clear the screen") == 0) {
        kprintf("\033[2J\033[H");
    } else if (strlen(line) > 0) {
        kprintf("I'm sorry, I don't understand \"%s\". Try \"help me please\".\n", line);
    }
}

void shell_init(void) {
    kprintf("\nWelcome to ZirvShell!\n");
    kprintf("Type \"help me please\" for a list of commands.\n\n");
}

void shell_run(void) {
    char line[SHELL_MAX_LINE];
    int pos = 0;

    shell_prompt();

    while (true) {
        char c = serial_getc(SERIAL_COM1);
        
        if (c == '\r' || c == '\n') {
            serial_puts(SERIAL_COM1, "\r\n");
            line[pos] = '\0';
            shell_execute(line);
            pos = 0;
            shell_prompt();
        } else if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                serial_puts(SERIAL_COM1, "\b \b");
            }
        } else if (pos < SHELL_MAX_LINE - 1) {
            line[pos++] = c;
            serial_putc(SERIAL_COM1, c);
        }
    }
}
