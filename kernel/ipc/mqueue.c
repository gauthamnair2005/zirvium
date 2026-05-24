#include "mqueue.h"
#include "kernel/proc/process.h"
#include "kernel/proc/scheduler.h"
#include "kernel/console.h"
#include <string.h>

extern void *kmalloc(size_t size, unsigned int flags);
extern void  kfree(void *ptr);
#include "kernel/console.h"
#include <string.h>

static mqueue_t *g_queues[MQ_MAX_QUEUES];
static uint32_t g_next_id = 1;

void mq_init(void) {
    memset(g_queues, 0, sizeof(g_queues));
    g_next_id = 1;
}

mqueue_t *mq_create(uint32_t *id_out) {
    for (int i = 0; i < MQ_MAX_QUEUES; i++) {
        if (!g_queues[i]) {
            mqueue_t *q = (mqueue_t *)kmalloc(sizeof(mqueue_t), 0);
            if (!q) return NULL;
            memset(q, 0, sizeof(mqueue_t));
            q->id = (uint32_t)(i + 1);
            g_queues[i] = q;
            if (id_out) *id_out = q->id;
            return q;
        }
    }
    return NULL;
}

mqueue_t *mq_get(uint32_t id) {
    if (id == 0 || id > MQ_MAX_QUEUES) return NULL;
    return g_queues[id - 1];
}

void mq_destroy(uint32_t id) {
    mqueue_t *q = mq_get(id);
    if (!q) return;
    if (q->waiting_proc) {
        q->waiting_proc->state = PROC_STATE_RUNNING;
        q->waiting_proc = NULL;
    }
    q->destroyed = true;
    g_queues[id - 1] = NULL;
    kfree(q);
}

int mq_send(uint32_t id, uint32_t sender_pid, uint32_t type, const void *data, uint32_t len) {
    mqueue_t *q = mq_get(id);
    if (!q || q->destroyed) return -1;
    if (len > MQ_MSG_SIZE) len = MQ_MSG_SIZE;
    if (q->count >= MQ_MAX_MSG) return -1;

    mq_message_t *msg = &q->msgs[q->tail];
    msg->sender_pid = sender_pid;
    msg->type = type;
    msg->data_len = len;
    if (data && len > 0)
        memcpy(msg->data, data, len);
    q->tail = (q->tail + 1) % MQ_MAX_MSG;
    q->count++;

    if (q->waiting_proc) {
        q->waiting_proc->state = PROC_STATE_RUNNING;
        q->waiting_proc = NULL;
    }
    return 0;
}

int mq_recv(uint32_t id, uint32_t *sender_pid, uint32_t *type, void *data, uint32_t *len, int block) {
    mqueue_t *q = mq_get(id);
    if (!q || q->destroyed) return -1;

    if (q->count == 0) {
        if (!block) return -1;
        process_t *proc = proc_current();
        if (!proc) return -1;
        q->waiting_proc = proc;
        proc->state = PROC_STATE_BLOCKED;
        sched_yield();
        if (q->destroyed) return -1;
        if (q->count == 0) return -1;
    }

    mq_message_t *msg = &q->msgs[q->head];
    if (sender_pid) *sender_pid = msg->sender_pid;
    if (type) *type = msg->type;
    if (data && msg->data_len > 0)
        memcpy(data, msg->data, msg->data_len > *len ? *len : msg->data_len);
    if (len) *len = msg->data_len;

    q->head = (q->head + 1) % MQ_MAX_MSG;
    q->count--;
    return 0;
}

void mq_wake_waiters(uint32_t id) {
    mqueue_t *q = mq_get(id);
    if (!q) return;
    if (q->waiting_proc) {
        q->waiting_proc->state = PROC_STATE_RUNNING;
        q->waiting_proc = NULL;
    }
}
