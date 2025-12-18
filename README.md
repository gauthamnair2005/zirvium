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
├── boot/           # UEFI bootloader (optional)
├── kernel/         # Kernel core
│   ├── arch/       # Architecture-specific code (x86)
│   ├── mm/         # Memory management module
│   ├── proc/       # Process management module
│   ├── fs/         # File system module
│   ├── drivers/    # Device drivers module
│   └── syscall/    # System call module
├── lib/            # Kernel libraries
├── include/        # Header files
├── scripts/        # Build scripts (menuconfig)
├── docs/           # Documentation
├── .config         # Kernel configuration
└── Kconfig         # Configuration definitions
```

## Build System

Zirvium uses a Linux-like build system:

### Configuration
```bash
make menuconfig     # Interactive configuration menu
make help           # Show all build targets
```

### Build Targets
```bash
make vmzirvium      # Build compressed kernel (default)
make iso            # Build bootable ISO
make clean          # Remove build artifacts
make mrproper       # Clean everything including .config
```

### Testing
```bash
make run            # Run in QEMU
make run-debug      # Run with debug output
```

## Documentation

- **README.md** - This file
- **QUICKSTART.md** - Getting started guide
- **docs/ARCHITECTURE.md** - System architecture
- **docs/BUILDING.md** - Build instructions
- **docs/BUILD_SYSTEM.md** - Modular build system guide
- **docs/SYSCALLS.md** - System call API reference
- **docs/DEVELOPMENT.md** - Developer guide

## License

MIT License
