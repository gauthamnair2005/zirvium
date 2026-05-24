#ifndef ZIRVIUM_KERNEL_IPC_MQUEUE_H
#define ZIRVIUM_KERNEL_IPC_MQUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MQ_MAX_MSG    256
#define MQ_MSG_SIZE   64
#define MQ_MAX_QUEUES 128

typedef struct {
    uint32_t sender_pid;
    uint32_t type;
    uint8_t  data[MQ_MSG_SIZE];
    uint32_t data_len;
} mq_message_t;

typedef struct mqueue {
    uint32_t     id;
    mq_message_t msgs[MQ_MAX_MSG];
    uint32_t     head;
    uint32_t     tail;
    uint32_t     count;
    bool         destroyed;
    struct process *waiting_proc;
} mqueue_t;

void     mq_init(void);
mqueue_t *mq_create(uint32_t *id_out);
void     mq_destroy(uint32_t id);
int      mq_send(uint32_t id, uint32_t sender_pid, uint32_t type, const void *data, uint32_t len);
int      mq_recv(uint32_t id, uint32_t *sender_pid, uint32_t *type, void *data, uint32_t *len, int block);
void     mq_wake_waiters(uint32_t id);
mqueue_t *mq_get(uint32_t id);

#endif
