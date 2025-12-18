# Zirvium OS - UNIX-like Kernel for x64

A modular UNIX-like operating system kernel designed for x86 architecture with Linux-like build system.

## Features

- **Modular Build System**: Linux-like Kconfig and menuconfig
- **Compressed Kernel**: vmzirvium image (similar to vmlinuz)
- **Memory Management**: Physical and virtual memory management with paging
- **Process Management**: Multi-tasking with process scheduling
- **System Calls**: UNIX-like system call interface (9 syscalls)
- **VFS**: Virtual File System layer
- **Device Drivers**: Modular device driver framework
- **Configuration**: Interactive menuconfig for kernel options

## Quick Start

### Configure
```bash
make menuconfig
```

### Build
```bash
make vmzirvium
```

### Run
```bash
make run
```

## Build Products

- **vmzirvium** - Compressed kernel image (3.4 KB)
- **zirvium** - Uncompressed ELF kernel (13 KB)
- **zirvium.iso** - Bootable ISO with GRUB

## Architecture

```
zirvium/
├── boot/           # UEFI bootloader
├── kernel/         # Kernel core
│   ├── arch/       # Architecture-specific code (x64)
│   ├── mm/         # Memory management
│   ├── proc/       # Process management
│   ├── fs/         # File system
│   ├── drivers/    # Device drivers
│   └── syscall/    # System call implementation
├── lib/            # Kernel libraries
└── include/        # Header files
```

## License

MIT License
