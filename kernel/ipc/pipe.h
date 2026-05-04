/* kernel/ipc/pipe.h
 * Zirvium Kernel — IPC pipe (ring buffer)
 *
 * Provides unidirectional byte-stream channels for inter-process
 * communication.  A pipe is created with pipe_create(); one side writes
 * with pipe_write() and the other reads with pipe_read().  When the write
 * end is closed, subsequent pipe_read() calls return 0 (EOF) once the
 * buffer is drained.
 *
 * This primitive is exposed to userspace via the SYS_PIPE syscall and is
 * the foundation for zirvlibc's communications standard library.
 */
#ifndef ZIRVIUM_KERNEL_IPC_PIPE_H
#define ZIRVIUM_KERNEL_IPC_PIPE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PIPE_BUF_SIZE  4096u   /* internal ring-buffer capacity */

typedef struct pipe {
    uint8_t  buf[PIPE_BUF_SIZE];
    uint32_t read_pos;
    uint32_t write_pos;
    uint32_t count;         /* bytes currently in the buffer */
    bool     read_closed;   /* read end has been closed */
    bool     write_closed;  /* write end has been closed */
} pipe_t;

/**
 * pipe_create — allocate and zero-initialise a new pipe.
 * Returns NULL on allocation failure.
 */
pipe_t *pipe_create(void);

/**
 * pipe_destroy — release all resources owned by @p.
 */
void pipe_destroy(pipe_t *p);

/**
 * pipe_write — copy up to @len bytes from @buf into the pipe.
 *
 * Returns the number of bytes actually written (may be less than @len if
 * the buffer is full), or -1 if the read end has been closed (broken pipe).
 */
int pipe_write(pipe_t *p, const void *buf, size_t len);

/**
 * pipe_read — copy up to @len bytes from the pipe into @buf.
 *
 * Returns the number of bytes read, 0 if the write end is closed and the
 * buffer is empty (EOF), or -1 on error.
 */
int pipe_read(pipe_t *p, void *buf, size_t len);

#endif /* ZIRVIUM_KERNEL_IPC_PIPE_H */
