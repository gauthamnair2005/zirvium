#ifndef ZIRVIUM_KERNEL_LOADER_ELF_H
#define ZIRVIUM_KERNEL_LOADER_ELF_H

#include <stdint.h>
#include <stdbool.h>
#include "kernel/proc/process.h"

/**
 * elf_load_process — loads a static ELF binary from a memory buffer into
 * a new process's address space.
 *
 * @proc:   The process to load into.
 * @buffer: Pointer to the ELF file in memory.
 *
 * Returns true on success, false if the buffer is not a valid Zirvium ELF.
 */
bool elf_load_process(process_t *proc, const void *buffer);

/**
 * elf_load_into_as — load a static ELF binary into the given address space.
 *
 * @as:      Target address space (user pages already cleared).
 * @buffer:  Pointer to the ELF file in memory.
 * @entry:   Output: entry point address.
 *
 * Returns true on success, false on invalid ELF.
 */
bool elf_load_into_as(address_space_t *as, const void *buffer, uint64_t *entry);

#endif /* ZIRVIUM_KERNEL_LOADER_ELF_H */