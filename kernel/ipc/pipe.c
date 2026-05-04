/* kernel/ipc/pipe.c
 * Zirvium Kernel — IPC pipe implementation
 */
#include "pipe.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Forward declarations for kernel heap (defined in kernel/mm/heap.c) */
extern void *kmalloc(size_t size, unsigned int flags);
extern void  kfree(void *ptr);

pipe_t *pipe_create(void)
{
    pipe_t *p = (pipe_t *)kmalloc(sizeof(pipe_t), 0);
    if (!p) return NULL;
    memset(p, 0, sizeof(pipe_t));
    return p;
}

void pipe_destroy(pipe_t *p)
{
    if (p) kfree(p);
}

int pipe_write(pipe_t *p, const void *buf, size_t len)
{
    if (!p || p->read_closed) return -1;

    const uint8_t *src = (const uint8_t *)buf;
    size_t written = 0;

    while (written < len && p->count < PIPE_BUF_SIZE) {
        p->buf[p->write_pos] = src[written++];
        p->write_pos = (p->write_pos + 1u) % PIPE_BUF_SIZE;
        p->count++;
    }
    return (int)written;
}

int pipe_read(pipe_t *p, void *buf, size_t len)
{
    if (!p) return -1;

    uint8_t *dst = (uint8_t *)buf;
    size_t nread = 0;

    while (nread < len && p->count > 0) {
        dst[nread++] = p->buf[p->read_pos];
        p->read_pos = (p->read_pos + 1u) % PIPE_BUF_SIZE;
        p->count--;
    }

    /* Return 0 (EOF) when write end is closed and buffer is empty */
    if (nread == 0 && p->write_closed)
        return 0;

    return (int)nread;
}
