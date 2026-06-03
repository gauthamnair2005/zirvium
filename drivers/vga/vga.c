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

/* UTF-8 decoder state for dropping multi-byte sequences gracefully.
 * VGA text mode (CP437) cannot display non-ASCII; we skip continuation bytes
 * and show '$' for the start byte as a visual fallback. */
static int  g_vga_utf8_left = 0;   /* remaining continuation bytes expected */

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

void vga_set_cursor(int row, int col)
{
    if (row < 0) row = 0;
    if (row >= VGA_ROWS) row = VGA_ROWS - 1;
    if (col < 0) col = 0;
    if (col >= VGA_COLS) col = VGA_COLS - 1;
    g_row = row;
    g_col = col;
    update_cursor();
}

int vga_get_cursor_row(void)
{
    return g_row;
}

int vga_get_cursor_col(void)
{
    return g_col;
}

int vga_get_rows(void)
{
    return VGA_ROWS;
}

int vga_get_cols(void)
{
    return VGA_COLS;
}

void vga_scroll(int lines)
{
    if (lines <= 0) return;
    if (lines >= VGA_ROWS) {
        vga_clear();
        return;
    }
    int remain = VGA_ROWS - lines;
    for (int r = 0; r < remain; r++) {
        for (int c = 0; c < VGA_COLS; c++) {
            g_buf[r * VGA_COLS + c] = g_buf[(r + lines) * VGA_COLS + c];
        }
    }
    uint16_t blank = (uint16_t)(' ' | ((uint16_t)g_color << 8));
    for (int r = remain; r < VGA_ROWS; r++) {
        for (int c = 0; c < VGA_COLS; c++) {
            g_buf[r * VGA_COLS + c] = blank;
        }
    }
    if (g_row >= remain) {
        g_row -= lines;
        if (g_row < 0) g_row = 0;
    }
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
    unsigned char uc = (unsigned char)c;

    /* UTF-8 multi-byte sequence handling:
     * VGA text mode (CP437) cannot render characters outside ASCII.
     * We skip continuation bytes (0x80-0xBF) and show '$' for the start
     * byte of multi-byte sequences as a visual placeholder. */
    if (g_vga_utf8_left > 0) {
        g_vga_utf8_left--;
        if ((uc & 0xC0) != 0x80)
            g_vga_utf8_left = 0;
        return;   /* drop continuation bytes silently */
    }
    if (uc >= 0x80) {
        if ((uc & 0xE0) == 0xC0) {
            g_vga_utf8_left = 1;
            uc = '$';
        } else if ((uc & 0xF0) == 0xE0) {
            g_vga_utf8_left = 2;
            uc = '$';
        } else if ((uc & 0xF8) == 0xF0) {
            g_vga_utf8_left = 3;
            uc = '$';
        } else if ((uc & 0xC0) == 0x80) {
            return;   /* stray continuation byte, drop */
        } else {
            return;   /* other non-ASCII, drop */
        }
    }

    switch (uc) {
    case '\n':
        g_col = 0;
        g_row++;
        break;
    case '\r':
        g_col = 0;
        break;
    case '\t':
        g_col = (g_col + 8) & ~7;
        if (g_col >= VGA_COLS) { g_col = 0; g_row++; }
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
            (uint16_t)((uint8_t)uc | ((uint16_t)g_color << 8));
        g_col++;
        if (g_col >= VGA_COLS) { g_col = 0; g_row++; }
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
