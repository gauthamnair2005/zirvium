# Building Zirvium OS

## Prerequisites

### Required Tools
- GCC (cross-compiler for x86_64-elf recommended)
- NASM (Netwide Assembler)
- GNU Make
- QEMU with OVMF (for testing)

### Ubuntu/Debian
```bash
sudo apt-get install build-essential nasm qemu-system-x86 ovmf
```

### Arch Linux
```bash
sudo pacman -S base-devel nasm qemu ovmf
```

### macOS
```bash
brew install nasm qemu
brew install x86_64-elf-gcc
```

## Building

### Compile the kernel
```bash
make all
```

This produces:
- `zirvium.elf` - Main kernel binary
- `bootx64.efi` - UEFI bootloader

### Clean build artifacts
```bash
make clean
```

## Running

### In QEMU
```bash
make run
```

This launches QEMU with:
- OVMF UEFI firmware
- 512MB RAM
- Kernel loaded directly

### Creating a bootable image

For a real UEFI boot (USB/disk):

```bash
# Create disk image
dd if=/dev/zero of=zirvium.img bs=1M count=64

# Create FAT32 filesystem
mkfs.vfat -F 32 zirvium.img

# Mount and copy files
mkdir -p mnt
sudo mount zirvium.img mnt
sudo mkdir -p mnt/EFI/BOOT
sudo cp bootx64.efi mnt/EFI/BOOT/BOOTX64.EFI
sudo cp zirvium.elf mnt/
sudo umount mnt

# Test in QEMU
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=zirvium.img,format=raw
```

## Debugging

### With GDB
```bash
# Terminal 1: Start QEMU with GDB server
qemu-system-x86_64 -s -S -bios /usr/share/ovmf/OVMF.fd -kernel zirvium.elf

# Terminal 2: Connect GDB
gdb zirvium.elf
(gdb) target remote :1234
(gdb) continue
```

### Serial Output
Add serial output for debugging:
```bash
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -kernel zirvium.elf -serial stdio
```

## Common Issues

### OVMF not found
If OVMF firmware is not at `/usr/share/ovmf/OVMF.fd`, find it:
```bash
find /usr -name "OVMF*.fd" 2>/dev/null
```
Update Makefile with correct path.

### Cross-compiler needed
For some systems, you may need a cross-compiler:
```bash
# Update Makefile
CC = x86_64-elf-gcc
```

### Page faults
Check memory mapping in VMM if kernel crashes on boot.
