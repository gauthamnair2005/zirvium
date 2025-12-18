# Zirvium Build System

## Overview

Zirvium uses a Linux-like modular build system with configuration management and compressed kernel images.

## Build Products

### vmzirvium
The main kernel image (similar to Linux's `vmlinuz`). This is a compressed or uncompressed kernel binary depending on configuration.

```bash
make vmzirvium
```

Output: `vmzirvium` (3-14 KB depending on compression)

### zirvium
Uncompressed ELF kernel image with debug symbols.

### zirvium.iso
Bootable ISO image with GRUB bootloader.

```bash
make iso
```

## Configuration System

### Interactive Configuration

Run the configuration menu:

```bash
make menuconfig
```

This provides an interactive TUI for configuring kernel options:
- General setup (compression, versioning)
- Processor features (64-bit, SMP)
- Memory management options
- Process management
- File systems
- Device drivers
- Debugging options

### Configuration File

The kernel configuration is stored in `.config`:

```bash
# View current configuration
cat .config

# Edit manually
vim .config
```

### Configuration Options

Key options in `.config`:

```makefile
CONFIG_COMPRESS_KERNEL=y       # Enable gzip compression
CONFIG_X86_64=y                # 64-bit kernel
CONFIG_SMP=n                   # SMP support
CONFIG_PHYSICAL_MEMORY_MANAGER=y
CONFIG_VIRTUAL_MEMORY_MANAGER=y
CONFIG_VFS=y                   # Virtual File System
CONFIG_CONSOLE_DRIVER=y        # Console support
CONFIG_DEBUG=n                 # Debugging features
```

## Kconfig System

Zirvium uses a Kconfig-like system (`Kconfig` file) to define configuration options with dependencies and help text.

Example from `Kconfig`:

```
config COMPRESS_KERNEL
    bool "Compress kernel image"
    default y
    help
      Compress the kernel image with gzip to reduce size.
      The bootloader will decompress it at boot time.
```

## Build Targets

### Standard Targets

```bash
make all          # Build vmzirvium (default)
make vmzirvium    # Build compressed kernel
make iso          # Build bootable ISO
make clean        # Remove built files
make mrproper     # Remove all generated files including config
```

### Configuration Targets

```bash
make menuconfig   # Interactive configuration
make config       # Reminder to edit .config
```

### Testing Targets

```bash
make run          # Run in QEMU
make run-debug    # Run in QEMU with debug output
```

### Installation

```bash
make install      # Shows installation instructions
```

Then manually:

```bash
sudo cp vmzirvium /boot/vmzirvium-0.1.0
sudo update-grub  # On systems with GRUB
```

## Modular Compilation

The build system conditionally compiles modules based on `.config`:

```makefile
ifeq ($(CONFIG_PHYSICAL_MEMORY_MANAGER),y)
    MM_OBJ += kernel/mm/pmm.o
endif
```

### Source Organization

```
kernel/
├── main.c              # Core kernel
├── console.c           # Console driver
├── arch/x64/           # Architecture-specific
├── mm/                 # Memory management module
├── proc/               # Process management module
├── fs/                 # File system module
├── syscall/            # System call module
└── drivers/            # Device drivers module
```

Each subdirectory is a "module" that can be optionally compiled.

## Compression

When `CONFIG_COMPRESS_KERNEL=y`:

1. Kernel is linked: `zirvium` (ELF)
2. Binary extracted: `zirvium.bin`
3. Compressed with gzip: `vmzirvium`

Result: ~75% size reduction

When `CONFIG_COMPRESS_KERNEL=n`:

1. Kernel is linked: `zirvium` (ELF)
2. Binary copied: `vmzirvium`

## GRUB Integration

The kernel is packaged with GRUB in an ISO:

```
isodir/
└── boot/
    ├── zirvium.elf     # Kernel
    └── grub/
        └── grub.cfg    # GRUB menu
```

GRUB configuration (`grub.cfg`):

```
menuentry "Zirvium OS" {
    multiboot2 /boot/zirvium.elf
    boot
}
```

## Build Process Flow

```
┌─────────────┐
│   Source    │
│   Files     │
└──────┬──────┘
       │
       ↓
┌─────────────┐
│  Compile    │ (.c → .o)
│   with CC   │
└──────┬──────┘
       │
       ↓
┌─────────────┐
│  Assemble   │ (.asm → .o)
│   with AS   │
└──────┬──────┘
       │
       ↓
┌─────────────┐
│    Link     │ (.o → zirvium)
│   with LD   │
└──────┬──────┘
       │
       ↓
┌─────────────┐
│  Extract    │ (zirvium → zirvium.bin)
│  with       │
│  objcopy    │
└──────┬──────┘
       │
       ↓ (if CONFIG_COMPRESS_KERNEL=y)
┌─────────────┐
│  Compress   │ (zirvium.bin → vmzirvium)
│  with gzip  │
└──────┬──────┘
       │
       ↓
┌─────────────┐
│ vmzirvium   │ ← Final kernel image
└─────────────┘
```

## Comparison with Linux

| Feature | Linux | Zirvium |
|---------|-------|---------|
| Kernel image | vmlinuz | vmzirvium |
| Config system | Kconfig + menuconfig | Simplified Kconfig + menuconfig.sh |
| Build tool | make + Kbuild | make |
| Compression | gzip/bzip2/xz/lzma | gzip |
| Modules | Loadable .ko | Built-in only |
| Init system | initramfs | None (planned) |

## Advanced Usage

### Custom Configuration

Create a custom config:

```bash
cp .config my-custom.config
make mrproper
cp my-custom.config .config
make vmzirvium
```

### Verbose Build

```bash
make V=1 vmzirvium
```

### Cross-compilation

```bash
make CC=x86_64-elf-gcc AS=nasm LD=x86_64-elf-ld
```

### Build Information

Check kernel version and features:

```bash
grep CONFIG_ .config
```

### Size Optimization

To reduce kernel size:

```bash
make menuconfig
# Disable unused drivers and features
# Enable compression
make vmzirvium
```

## Troubleshooting

### Build Errors

```bash
make clean
make menuconfig  # Verify configuration
make vmzirvium
```

### Missing Dependencies

```bash
# Install required tools
sudo apt install build-essential nasm gzip

# For ISO generation
sudo apt install grub-pc-bin xorriso
```

### Configuration Issues

```bash
# Reset to defaults
cp .config.default .config

# Or start fresh
make mrproper
make menuconfig
```

## Future Enhancements

- Loadable kernel modules (.ko files)
- Multiple compression formats (bzip2, xz, lz4)
- Parallel build support (make -j)
- Dependency tracking
- Out-of-tree module building
- menuconfig with ncurses TUI
- defconfig presets
