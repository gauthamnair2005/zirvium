#include "hpc.h"
#include "kernel/ipc/mqueue.h"
#include "kernel/proc/process.h"
#include "kernel/proc/scheduler.h"
#include "kernel/console.h"
#include <string.h>

extern void *kmalloc(size_t size, unsigned int flags);
extern void  kfree(void *ptr);

static int g_hpc_initialized = 0;
static uint32_t g_barrier_mq = 0;
static uint32_t g_hpc_mq_base = 0;

static void assign_rank(process_t *proc) {
    if (!proc) return;
    static int next_rank_val = 0;
    proc->hpc_rank = next_rank_val++;
    proc->hpc_enabled = 1;
}

int hpc_init(void) {
    if (g_hpc_initialized) return 0;
    mq_init();
    uint32_t barrier_id;
    mqueue_t *bmq = mq_create(&barrier_id);
    if (!bmq) return -1;
    g_barrier_mq = barrier_id;

    uint32_t base_id;
    mqueue_t *hmq = mq_create(&base_id);
    if (!hmq) return -1;
    g_hpc_mq_base = base_id;

    g_hpc_initialized = 1;
    klog(LOG_OK, "HPC", "HPC subsystem initialised");
    return 0;
}

int hpc_rank(void) {
    process_t *proc = proc_current();
    if (!proc) return -1;
    if (!proc->hpc_enabled) {
        proc->hpc_enabled = 1;
        assign_rank(proc);
    }
    return proc->hpc_rank;
}

int hpc_size(void) {
    int count = 0;
    process_t *p = proc_list;
    while (p) {
        if (p->hpc_enabled) count++;
        p = p->next;
    }
    return count > 0 ? count : 1;
}

int hpc_barrier(void) {
    if (!g_hpc_initialized) return -1;
    process_t *proc = proc_current();
    if (!proc) return -1;
    if (!proc->hpc_enabled) return 0;

    int expected = hpc_size();
    proc->hpc_barrier_count = 1;

    mqueue_t *bmq = mq_get(g_barrier_mq);
    if (!bmq) return -1;

    mq_send(g_barrier_mq, proc->pid, 0, NULL, 0);

    uint32_t total_msgs = 0;
    while (total_msgs < (uint32_t)expected) {
        uint32_t sp, tp, lenp;
        uint8_t tmp[4];
        lenp = sizeof(tmp);
        if (mq_recv(g_barrier_mq, &sp, &tp, tmp, &lenp, 1) == 0)
            total_msgs++;
    }
    return 0;
}

int hpc_send(int dest_rank, const void *data, size_t len, int tag) {
    process_t *proc = proc_current();
    if (!proc) return -1;
    uint32_t mq_id = g_hpc_mq_base + (uint32_t)dest_rank;
    mqueue_t *q = mq_get(mq_id);
    if (!q) {
        uint32_t new_id;
        mqueue_t *nq = mq_create(&new_id);
        if (!nq) return -1;
        g_hpc_mq_base = new_id;
        mq_id = new_id + (uint32_t)dest_rank;
    }
    return mq_send(mq_id, proc->pid, (uint32_t)tag, data, (uint32_t)len);
}

int hpc_recv(int src_rank, void *buf, size_t len, int tag) {
    uint32_t mq_id = g_hpc_mq_base + (uint32_t)src_rank;
    mqueue_t *q = mq_get(mq_id);
    if (!q) return -1;

    uint32_t sender_pid, type, data_len = (uint32_t)len;
    int ret = mq_recv(mq_id, &sender_pid, &type, buf, &data_len, 1);
    if (ret == 0 && (int)type != tag) return -1;
    return ret;
}

int hpc_bcast(int root_rank, void *data, size_t len) {
    int rank = hpc_rank();
    int sz = hpc_size();
    if (rank == root_rank) {
        for (int r = 0; r < sz; r++) {
            if (r != rank)
                hpc_send(r, data, len, 0);
        }
    } else {
        hpc_recv(root_rank, data, len, 0);
    }
    return 0;
}

int hpc_reduce(int root_rank, const void *src, void *dst, size_t len, int op) {
    int rank = hpc_rank();
    int sz = hpc_size();

    if (rank == root_rank) {
        memcpy(dst, src, len);
        for (int r = 0; r < sz; r++) {
            if (r == rank) continue;
            uint8_t buf[256];
            size_t rlen = len > sizeof(buf) ? sizeof(buf) : len;
            if (hpc_recv(r, buf, rlen, 0) == 0) {
                for (size_t i = 0; i < rlen; i++) {
                    switch (op) {
                    case HPC_OP_SUM: ((uint8_t *)dst)[i] += buf[i]; break;
                    case HPC_OP_MAX: if (buf[i] > ((uint8_t *)dst)[i]) ((uint8_t *)dst)[i] = buf[i]; break;
                    case HPC_OP_MIN: if (buf[i] < ((uint8_t *)dst)[i]) ((uint8_t *)dst)[i] = buf[i]; break;
                    }
                }
            }
        }
    } else {
        hpc_send(root_rank, src, len, 0);
    }
    return 0;
}
