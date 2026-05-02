# Zirvium Kernel

A kernel written from scratch for **x86-64** devices that runs the full
**Zirvium stack** (no GNU tools, no libc).  
It implements the **MOSIX** (Modern OSIX) standard instead of POSIX.

---

## MOSIX Filesystem Hierarchy

MOSIX replaces the traditional POSIX/FHS layout with a simpler, purpose-driven
structure:

| Path | Purpose |
|------|---------|
| `/bin` | Executable binaries **only** (no libraries, execute permission required) |
| `/lib` | Non-executable library (`.a`/`.so`) and header files |
| `/user` | User home directories — replaces `/home` (NOT `/usr`) |
| `/boot` | Bootloader and kernel images (same role as POSIX `/boot`) |
| `/config` | Machine/human-readable configuration files — replaces `/etc` |
| `/zirv` | Virtual device namespace — replaces `/dev`, `/run`, `/sys` |
| `/mounts` | Mount points for removable / additional filesystems — replaces `/mnt` |
| `/tmp` | Ephemeral temporary files (same as Linux `/tmp`) |

---

## /zirv Device Namespace

Every physical device is identified by a canonical path that encodes the
**bus technology** and **media type**:

```
/zirv/<bus>/<type><index>
```

| Bus | Media type | Example path |
|-----|-----------|--------------|
| `sata` | `hdd` | `/zirv/sata/hdd0` |
| `sata` | `ssd` | `/zirv/sata/ssd0` |
| `sata` | `cdrom` | `/zirv/sata/cdrom0` |
| `sata` | `cdrw` | `/zirv/sata/cdrw0` |
| `pata` | `hdd` | `/zirv/pata/hdd0` |
| `pata` | `ssd` | `/zirv/pata/ssd0` |
| `pata` | `cdrom` | `/zirv/pata/cdrom0` |
| `pata` | `cdrw` | `/zirv/pata/cdrw0` |
| `nvme` | `ssd` | `/zirv/nvme/ssd0` |
| `usb` | `pendrive` | `/zirv/usb/pendrive0` |
| `usb` | `hdd` | `/zirv/usb/hdd0` |
| `net` | `eth` | `/zirv/net/eth0` |
| `net` | `wlan` | `/zirv/net/wlan0` |
| `input` | `keyboard` | `/zirv/input/keyboard0` |
| `display` | `framebuffer` | `/zirv/display/framebuffer0` |
| `tty` | `serial` | `/zirv/tty/serial0` |

---

## Repository Layout

```
zirvium/
├── arch/
│   └── x64/
│       ├── boot.asm          # Multiboot2 entry, 32→64-bit transition, paging
│       ├── cpu.h             # Port I/O, CR/MSR access, RDTSC
│       ├── gdt.h / gdt.c     # GDT + TSS setup
│       └── idt.h / idt.c     # IDT + ISR dispatch
│           isr_stubs.asm     # 256 ISR stub entry-points
├── kernel/
│   ├── main.c                # kernel_main(): boot sequence orchestrator
│   ├── kernel.ld             # Linker script (loads at 1 MiB, links at −2 GiB)
│   └── mm/
│       ├── pmm.h / pmm.c     # Physical memory manager (bitmap allocator)
│       └── vmm.h / vmm.c     # Virtual memory manager (4-level paging)
├── fs/
│   ├── mosix.h               # MOSIX VFS types, directory constants, public API
│   └── vfs.c                 # In-memory VFS tree + /zirv namespace
├── drivers/
│   ├── serial/
│   │   ├── serial.h
│   │   └── serial.c          # 16550 UART driver (115200 8N1)
│   └── zirv/
│       ├── device.h / device.c  # /zirv device registry
│       ├── sata.c            # ATA PIO driver → /zirv/sata/*
│       ├── nvme.h / nvme.c   # NVMe PCI driver → /zirv/nvme/*
│       └── usb_storage.h / usb_storage.c  # USB MSD stub → /zirv/usb/*
├── Makefile
└── README.md
```

---

## Building

### Prerequisites

| Tool | Purpose |
|------|---------|
| `x86_64-elf-gcc` | Freestanding C compiler |
| `nasm ≥ 2.14` | Assembler |
| `x86_64-elf-ld` | Linker |
| `grub-mkrescue` + `xorriso` | ISO creation (optional) |
| `qemu-system-x86_64` | Testing (optional) |

```bash
# Build the kernel ELF
make

# Build a GRUB2 bootable ISO
make iso

# Run under QEMU (serial output goes to terminal)
make run

# Debug with GDB
make debug
```

---

## Boot Sequence

1. **Multiboot2 header** (`arch/x64/boot.asm`) — loaded by GRUB2
2. CPUID checks for long-mode support
3. Identity page-tables for the first 4 GiB (2 MiB huge pages)
4. Switch to 64-bit long mode, load 64-bit GDT, far-jump
5. `kernel_main()` is called with multiboot2 magic + info pointer
6. Serial console initialised (COM1, 115200 baud)
7. Full GDT (with TSS) and IDT installed
8. Multiboot2 memory map parsed → PMM initialised
9. VMM sets up 4-level paging with kernel direct map
10. MOSIX VFS namespace created (`/bin`, `/lib`, `/user`, `/boot`, `/config`, `/zirv`, `/mounts`, `/tmp`)
11. Device registry initialised; SATA/PATA, NVMe and USB storage probed
12. Interrupts enabled; idle loop

---

## License

See [LICENSE](LICENSE).
