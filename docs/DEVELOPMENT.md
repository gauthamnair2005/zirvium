# Development Guide

## Project Structure

```
zirvium/
├── boot/              # UEFI bootloader
│   └── boot.c         # EFI application entry
├── kernel/            # Kernel source
│   ├── arch/          # Architecture-specific code
│   │   └── x64/       # x86-64 implementation
│   ├── fs/            # File systems
│   ├── mm/            # Memory management
│   ├── proc/          # Process management
│   ├── syscall/       # System call handlers
│   ├── drivers/       # Device drivers
│   ├── console.c      # Console output
│   └── main.c         # Kernel entry point
├── include/           # Header files
│   ├── arch/          # Architecture headers
│   └── kernel/        # Kernel headers
├── lib/               # Kernel libraries
│   └── string.c       # String utilities
└── docs/              # Documentation
```

## Adding a New System Call

1. Define system call number in `include/kernel/syscall.h`:
```c
#define SYS_MYNEWCALL 9
```

2. Implement handler in `kernel/syscall/syscall.c`:
```c
static int64_t sys_mynewcall(uint64_t arg1) {
    // Implementation
    return 0;
}
```

3. Add to syscall_handler switch:
```c
case SYS_MYNEWCALL:
    return sys_mynewcall(arg1);
```

## Adding a Device Driver

1. Create driver file in `kernel/drivers/`:
```c
// kernel/drivers/mydevice.c
#include <kernel/kernel.h>

void mydevice_init(void) {
    // Initialize device
}

void mydevice_read(void *buffer, size_t size) {
    // Read from device
}
```

2. Add header in `include/kernel/`:
```c
// include/kernel/mydevice.h
#ifndef KERNEL_MYDEVICE_H
#define KERNEL_MYDEVICE_H

void mydevice_init(void);
void mydevice_read(void *buffer, size_t size);

#endif
```

3. Call init function in `kernel/main.c`:
```c
#include <kernel/mydevice.h>

void kernel_main(void) {
    // ...
    mydevice_init();
    kprintf("[OK] My device initialized\n");
    // ...
}
```

## Adding a File System

1. Create FS implementation in `kernel/fs/`:
```c
// kernel/fs/myfs.c
#include <kernel/vfs.h>

static ssize_t myfs_read(vfs_node_t *node, uint64_t offset, 
                         uint64_t size, uint8_t *buffer) {
    // Read implementation
    return size;
}

void myfs_mount(const char *path) {
    vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
    // Set up node
    node->read = myfs_read;
    // Register with VFS
}
```

## Memory Management

### Physical Memory Manager (PMM)
- Bitmap-based allocator
- 4KB page granularity
- Functions: `pmm_alloc()`, `pmm_free()`

### Virtual Memory Manager (VMM)
- 4-level paging (PML4)
- Higher-half kernel
- Functions: `vmm_alloc()`, `vmm_map()`, `vmm_free()`

### Kernel Heap
- Simple first-fit allocator
- Functions: `kmalloc()`, `kfree()`

## Debugging Tips

### Print debugging
```c
kprintf("Debug: value = %d\n", my_value);
kprintf("Pointer: 0x%x\n", my_pointer);
```

### Check register state
```c
uint64_t rsp;
asm volatile("mov %%rsp, %0" : "=r"(rsp));
kprintf("RSP = 0x%x\n", rsp);
```

### Panic on error
```c
if (error_condition) {
    kernel_panic("Something went wrong!");
}
```

## Testing

### Unit Testing
Currently no unit test framework. Tests should be added to kernel initialization.

### Integration Testing
Test in QEMU:
```bash
make run
```

### Manual Testing
1. Boot kernel in QEMU
2. Verify initialization messages
3. Test specific features

## Code Style

- Use 4-space indentation
- K&R brace style
- Descriptive variable names
- Comment complex logic
- Keep functions under 50 lines when possible

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make changes
4. Test thoroughly
5. Submit pull request

## Roadmap

### Short Term
- [ ] Keyboard driver
- [ ] Timer/PIT driver
- [ ] Better process scheduler
- [ ] ELF loader

### Medium Term
- [ ] User space support
- [ ] Shell implementation
- [ ] Basic file system (initramfs)
- [ ] System V IPC

### Long Term
- [ ] Disk drivers (ATA/AHCI)
- [ ] ext2 file system
- [ ] Network stack
- [ ] Multi-core support
