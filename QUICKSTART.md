# Zirvium OS - Quick Start Guide

Welcome to Zirvium OS! This guide will help you get started quickly.

## What is Zirvium?

Zirvium is a minimal UNIX-like operating system kernel for x86-64 architecture with UEFI boot support. It features:

- ✅ UEFI bootloader
- ✅ x86-64 architecture (64-bit)
- ✅ Memory management (physical & virtual)
- ✅ Process management with scheduling
- ✅ UNIX-like system calls
- ✅ Virtual File System (VFS)
- ✅ Console I/O
- ✅ ~1000 lines of well-structured code

## Prerequisites

Install the required tools:

**Ubuntu/Debian:**
```bash
sudo apt install build-essential nasm qemu-system-x86 ovmf
```

**Arch Linux:**
```bash
sudo pacman -S base-devel nasm qemu ovmf
```

**macOS:**
```bash
brew install nasm qemu gcc
```

## Build & Run

```bash
# Clone or navigate to the project
cd zirvium

# Build the kernel
make all

# Run in QEMU
make run
```

You should see:
```
Zirvium OS v0.1.0
Initializing kernel...
[OK] GDT initialized
[OK] IDT initialized
[OK] Memory management initialized
[OK] Virtual file system initialized
[OK] System calls initialized
[OK] Process management initialized
[OK] Interrupts enabled

Kernel initialization complete!
Welcome to Zirvium OS
```

## Project Structure

```
zirvium/
├── boot/              # UEFI bootloader
├── kernel/            # Kernel implementation
│   ├── arch/x64/      # x86-64 specific code
│   ├── mm/            # Memory management
│   ├── proc/          # Process management
│   ├── fs/            # File system
│   ├── syscall/       # System calls
│   ├── main.c         # Kernel entry
│   └── console.c      # Console driver
├── include/           # Header files
├── lib/               # Kernel libraries
└── docs/              # Documentation
```

## Key Features Explained

### 1. UEFI Boot
Modern UEFI bootloader that works on real hardware and VMs.

### 2. Memory Management
- **PMM**: Physical memory manager with bitmap allocation
- **VMM**: Virtual memory with 4-level paging
- **Heap**: Kernel heap allocator (kmalloc/kfree)

### 3. Process Management
- Process creation and scheduling
- Process states (RUNNING, READY, BLOCKED, ZOMBIE)
- Basic round-robin scheduler

### 4. System Calls
UNIX-like interface:
- File operations: read, write, open, close
- Process control: fork, exec, exit, getpid, kill

### 5. Virtual File System
Abstract file system layer supporting multiple FS types.

## Next Steps

1. **Read the docs:**
   - `docs/ARCHITECTURE.md` - System architecture
   - `docs/BUILDING.md` - Build instructions
   - `docs/SYSCALLS.md` - System call reference
   - `docs/DEVELOPMENT.md` - Development guide

2. **Explore the code:**
   - Start with `kernel/main.c`
   - Check out `kernel/mm/` for memory management
   - Look at `kernel/syscall/` for system calls

3. **Extend the kernel:**
   - Add a keyboard driver
   - Implement a timer
   - Create a simple shell
   - Add more system calls

## Debugging

### Enable debug output:
```bash
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd \
  -kernel zirvium.elf -serial stdio -d int,cpu_reset
```

### Use GDB:
```bash
# Terminal 1
qemu-system-x86_64 -s -S -bios /usr/share/ovmf/OVMF.fd -kernel zirvium.elf

# Terminal 2
gdb zirvium.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

## Common Issues

**Q: "OVMF.fd not found"**
A: Find OVMF location: `find /usr -name "OVMF*.fd"`

**Q: "make: nasm: Command not found"**
A: Install NASM: `sudo apt install nasm`

**Q: Kernel doesn't boot**
A: Check that GCC targets x86_64. You may need a cross-compiler.

## Resources

- [OSDev Wiki](https://wiki.osdev.org/) - OS development resources
- [Intel Manuals](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html) - x86 documentation
- [UEFI Specification](https://uefi.org/specifications) - UEFI reference

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

MIT License - See LICENSE file for details.

## Support

For questions or issues:
- Open an issue on GitHub
- Check the documentation in `docs/`
- Review the code comments

---

**Happy hacking!** 🚀
