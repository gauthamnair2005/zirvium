/* drivers/vga/vga.c
 * Zirvium Kernel — VGA text-mode console driver
 *
 * The VGA text buffer lives at physical 0xB8000.  We access it through the
 * direct physical map (PHYS_MAP_BASE) so it remains accessible after
 * switching to a process address space that does NOT have an identity map.
 *
 * Hardware cursor position is updated via the standard CRT controller ports
 * (0x3D4/0x3D5) after every character write.
 *
 * NOTE: vga_init() must be called AFTER vmm_init() so that the physical map
 * at PHYS_MAP_BASE is active.
 */
#include "vga.h"
#include "arch/x64/cpu.h"
#include "kernel/mm/vmm.h"
#include <stdint.h>
#include <stddef.h>

/* ── CRT controller I/O ports ─────────────────────────────────────────────── */
#define VGA_CTRL_PORT        0x3D4u
#define VGA_DATA_PORT        0x3D5u
#define VGA_REG_CURSOR_HIGH  0x0Eu
#define VGA_REG_CURSOR_LOW   0x0Fu

/* ── Driver state ─────────────────────────────────────────────────────────── */
static volatile uint16_t *const g_buf =
    (volatile uint16_t *)(uintptr_t)(PHYS_MAP_BASE + VGA_BUFFER_PHYS);

static int     g_col   = 0;
static int     g_row   = 0;
static uint8_t g_color = 0;   /* set in vga_init */

/* ── Helpers ──────────────────────────────────────────────────────────────── */
static void update_cursor(void)
{
    uint16_t pos = (uint16_t)(g_row * VGA_COLS + g_col);
    outb(VGA_CTRL_PORT, VGA_REG_CURSOR_HIGH);
    outb(VGA_DATA_PORT, (uint8_t)(pos >> 8));
    outb(VGA_CTRL_PORT, VGA_REG_CURSOR_LOW);
    outb(VGA_DATA_PORT, (uint8_t)(pos & 0xFFu));
}

static void scroll_up(void)
{
    /* Move rows 1‥24 up by one row */
    for (int r = 1; r < VGA_ROWS; r++) {
        for (int c = 0; c < VGA_COLS; c++) {
            g_buf[(r - 1) * VGA_COLS + c] = g_buf[r * VGA_COLS + c];
        }
    }
    /* Blank the last row */
    uint16_t blank = (uint16_t)(' ' | ((uint16_t)g_color << 8));
    for (int c = 0; c < VGA_COLS; c++)
        g_buf[(VGA_ROWS - 1) * VGA_COLS + c] = blank;
    g_row = VGA_ROWS - 1;
}

/* ── Public API ───────────────────────────────────────────────────────────── */
void vga_set_color(uint8_t color)
{
    g_color = color;
}

void vga_clear(void)
{
    uint16_t blank = (uint16_t)(' ' | ((uint16_t)g_color << 8));
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        g_buf[i] = blank;
    g_col = 0;
    g_row = 0;
    update_cursor();
}

void vga_init(void)
{
    g_color = VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK);
    g_col   = 0;
    g_row   = 0;
    vga_clear();
}

void vga_putc(char c)
{
    switch (c) {
    case '\n':
        g_col = 0;
        g_row++;
        break;
    case '\r':
        g_col = 0;
        break;
    case '\t':
        /* Advance to next 8-column tab stop */
        g_col = (g_col + 8) & ~7;
        if (g_col >= VGA_COLS) {
            g_col = 0;
            g_row++;
        }
        break;
    case '\b':
        if (g_col > 0) {
            g_col--;
            g_buf[g_row * VGA_COLS + g_col] =
                (uint16_t)(' ' | ((uint16_t)g_color << 8));
        }
        break;
    default:
        g_buf[g_row * VGA_COLS + g_col] =
            (uint16_t)((uint8_t)c | ((uint16_t)g_color << 8));
        g_col++;
        if (g_col >= VGA_COLS) {
            g_col = 0;
            g_row++;
        }
        break;
    }

    if (g_row >= VGA_ROWS)
        scroll_up();

    update_cursor();
}

void vga_puts(const char *s)
{
    for (; *s; s++)
        vga_putc(*s);
}
