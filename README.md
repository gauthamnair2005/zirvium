# Zirvium Kernel

A kernel written from scratch for **x86-64** devices that runs the full **Zirvium stack** (no GNU libc).  
It implements the **MOSIX** (Modern OSIX) standard instead of POSIX.

---

## MOSIX Filesystem Hierarchy

MOSIX replaces the traditional POSIX/FHS layout with a simpler, purpose-driven structure:

| Path | Purpose |
|------|---------|
| `/bin` | Executable binaries only (execute permission required) |
| `/lib` | Non-executable library (`.a`/`.so`) and header files |
| `/user` | User home directories — replaces `/home` (NOT `/usr`) |
| `/boot` | Bootloader and kernel images |
| `/config` | Machine/human-readable config — replaces `/etc` |
| `/zirv` | Virtual device namespace — replaces `/dev`, `/run`, `/sys` |
| `/mounts` | Mount points for removable filesystems — replaces `/mnt` |
| `/tmp` | Ephemeral temporary files |

---

## /zirv Device Namespace

```
/zirv/<bus>/<type><index>
```

| Bus | Media types |
|-----|-------------|
| `sata` | `hdd`, `ssd`, `cdrom`, `cdrw` |
| `pata` | `hdd`, `ssd`, `cdrom`, `cdrw` |
| `nvme` | `ssd` |
| `usb` | `pendrive`, `hdd` |
| `net` | `eth`, `wlan` |
| `input` | `keyboard`, `mouse`, `touchpad` |
| `display` | `framebuffer`, `gpu` |
| `tty` | `serial`, `virtual` |
| `audio` | `output`, `input` |

---

## Repository Layout

```
zirvium/
├── arch/x64/          # Boot, GDT, IDT, CPU helpers, ISR stubs, syscall entry
├── kernel/
│   ├── main.c          # Boot orchestrator
│   ├── mm/             # PMM (bitmap), VMM (4-level paging), heap
│   ├── proc/           # Process management, usermode entry
│   ├── syscall/        # Syscall dispatch table
│   ├── irq/            # 8259A PIC + IRQ handler registration
│   ├── ipc/            # Pipe implementation
│   └── loader/         # ELF loader, embedded binary table, init_bin.asm
├── fs/                 # MOSIX VFS tree, console device node
├── drivers/
│   ├── serial/         # 16550 UART (COM1, 115200 8N1)
│   ├── vga/            # VGA text-mode console (PHYS_MAP_BASE)
│   ├── pci/            # PCI bus scanner
│   └── zirv/
│       ├── input/ps2/  # i8042 controller + PS/2 keyboard decoder
│       ├── display/    # Bochs VGA, VMware SVGA, i915
│       ├── net/        # Intel E1000, VirtIO-Net, RTL8139
│       └── ...         # SATA, NVMe, USB, VirtIO-Blk, TPM, audio
├── libs/zirvlibc/      # Freestanding C library (submodule)
├── zirvinit/           # PID 1 init process (submodule)
├── zirvshell/          # Interactive shell (submodule)
├── zirvutils/          # System utilities (submodule)
└── Makefile
```

---

## Embedded Binaries

Userspace programs are compiled as statically linked, no-pie ELFs and embedded into the kernel via `incbin`:

| Path | Source | Description |
|------|--------|-------------|
| `/bin/init` | `zirvinit/` | PID 1 — launches shell |
| `/bin/shell` | `zirvshell/` | Interactive shell |
| `/bin/hello` | `zirvutils/` | Test utility |
| `/bin/cat` | `zirvutils/` | File reader |
| `/bin/sysinfo` | `zirvutils/` | System info |
| `/bin/clear` | `zirvutils/` | Clear screen |
| `/bin/echo` | `zirvutils/` | Print arguments |

Run them from the shell: `run /bin/hello`

---

## Building

### Prerequisites

| Tool | Purpose |
|------|---------|
| `x86_64-elf-gcc` (or `gcc`) | Freestanding C compiler |
| `nasm >= 2.14` | Assembler |
| `x86_64-elf-ld` (or `ld`) | Linker |
| `grub-mkrescue` + `xorriso` | ISO creation |
| `qemu-system-x86_64` | Testing |

```bash
# Build everything
make

# GRUB2 bootable ISO
make iso

# Run under QEMU (serial → terminal, VGA → SDL window)
make run

# Debug with GDB
make debug
```

---

## Boot Sequence

1. **Multiboot2** header loaded by GRUB
2. CPUID checks for long-mode, PAE
3. Identity page-tables (2 MiB huge pages, 0–4 GiB) + higher-half kernel map
4. 64-bit long mode, GDT loaded, far-jump to C code
5. `kernel_main()`: serial init → GDT/TSS/IDT/PIC → PMM → VMM → VGA console
6. MOSIX VFS namespace created (`/bin`, `/lib`, `/user`, etc.)
7. VFS console (`/zirv/tty/virtual0`) with keyboard + serial read
8. PCI bus scan + driver probing (VirtIO, E1000, RTL8139, etc.)
9. PS/2 controller + keyboard init (i8042 IRQ1 driver)
10. Embedded binary table populated
11. Syscall init (LSTAR MSR → `syscall_entry`)
12. PID 1 created, `zirvinit.elf` loaded, `proc_enter_usermode()` via IRETQ
13. Init execve's into shell → interactive MOSIX shell
