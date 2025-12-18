# Zirvium OS - UNIX-like Kernel for x64 UEFI

A minimal UNIX-like operating system kernel designed for x64 architecture with UEFI boot support.

## Features

- **UEFI Boot Support**: Native UEFI bootloader
- **Memory Management**: Physical and virtual memory management with paging
- **Process Management**: Multi-tasking with process scheduling
- **System Calls**: UNIX-like system call interface
- **VFS**: Virtual File System layer
- **Device Drivers**: Basic device driver framework
- **Shell**: Simple command-line interface

## Building

```bash
make all
```

## Running

```bash
make run
```

Note: Currently uses QEMU multiboot loading. UEFI bootloader is optional.

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
