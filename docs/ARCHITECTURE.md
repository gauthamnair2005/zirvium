# Zirvium OS Architecture

## Overview

Zirvium is a minimal UNIX-like kernel for x86-64 architecture with UEFI boot support. It implements fundamental operating system concepts including memory management, process scheduling, and a virtual file system.

## Memory Layout

```
0x0000000000000000 - 0x00007FFFFFFFFFFF : User space
0xFFFF800000000000 - 0xFFFF8FFFFFFFFFFF : Physical memory mapping
0xFFFFFFFF80000000 - 0xFFFFFFFFFFFFFFFF : Kernel space
```

## Components

### Boot (boot/)
- **boot.c**: UEFI bootloader that loads the kernel

### Architecture (kernel/arch/x64/)
- **boot.asm**: Initial kernel entry point and stack setup
- **gdt.c/gdt_flush.asm**: Global Descriptor Table initialization
- **idt.c/idt_flush.asm**: Interrupt Descriptor Table setup
- **isr.asm**: Interrupt Service Routines
- **io.c**: Port I/O operations

### Memory Management (kernel/mm/)
- **pmm.c**: Physical memory manager using bitmap allocation
- **vmm.c**: Virtual memory manager with 4-level paging
- **kmalloc.c**: Kernel heap allocator
- **mm.c**: Memory management initialization

### Process Management (kernel/proc/)
- **process.c**: Process creation, scheduling, and management
  - Round-robin scheduler
  - Process states: RUNNING, READY, BLOCKED, ZOMBIE
  - Fork/exec system call support (stub)

### File System (kernel/fs/)
- **vfs.c**: Virtual File System layer
  - Abstract file operations
  - Mount point support
  - File descriptor management

### System Calls (kernel/syscall/)
- **syscall.c**: System call interface
  - read, write, open, close
  - fork, exec, exit
  - getpid, kill

### Device Drivers (kernel/drivers/)
- Framework for device drivers
- Console driver integrated

## Build System

The Makefile compiles:
1. Assembly files with NASM
2. C files with GCC (freestanding)
3. Links with custom linker script
4. Produces ELF kernel binary

## Boot Process

1. UEFI firmware loads bootx64.efi
2. Bootloader prints message and jumps to kernel
3. Kernel initializes:
   - GDT (segmentation)
   - IDT (interrupts)
   - Memory management (PMM, VMM)
   - VFS (file system)
   - System calls
   - Process management
4. Enters idle loop

## Design Principles

- **Minimal**: Only essential features
- **Modular**: Clean separation of concerns
- **Educational**: Clear, readable code
- **UNIX-like**: Familiar system call interface
- **x86-64**: 64-bit architecture support

## Future Extensions

- Device drivers (keyboard, disk, network)
- ELF loader for user programs
- IPC mechanisms
- Advanced scheduler
- File system implementations (ext2, FAT32)
- Network stack
