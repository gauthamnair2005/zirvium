#ifndef ZIRVIUM_KERNEL_HPC_HPC_H
#define ZIRVIUM_KERNEL_HPC_HPC_H

#include <stdint.h>
#include <stddef.h>

#define HPC_MAX_PROCS 64

#define HPC_OP_SUM 0
#define HPC_OP_MAX 1
#define HPC_OP_MIN 2

int  hpc_init(void);
int  hpc_rank(void);
int  hpc_size(void);
int  hpc_barrier(void);
int  hpc_send(int dest_rank, const void *data, size_t len, int tag);
int  hpc_recv(int src_rank, void *buf, size_t len, int tag);
int  hpc_bcast(int root_rank, void *data, size_t len);
int  hpc_reduce(int root_rank, const void *src, void *dst, size_t len, int op);

#endif
