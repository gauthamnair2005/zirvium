# Zirvium Kernel — Reference MOSIX Implementation

A from-scratch **x86-64** kernel that implements the **MOSIX** (Modern OSIX)
operating system standard. Zirvium is to MOSIX what the Linux kernel is to
POSIX — the reference implementation of the specification.

| | |
|---|---|
| **Website** | [zirvworld](https://github.com/gauthamnair2005/zirvworld) — MOSIX standard & docs |
| **MOSIX spec** | Filesystem hierarchy, /zirv namespace, syscall ABI |
| **License** | GPLv3 |

---

## What is MOSIX?

**MOSIX** (Modern OSIX) is an open OS standard that replaces the traditional
POSIX/FHS model with a simplified, modern design. The complete specification
is maintained at [zirvworld](https://github.com/gauthamnair2005/zirvworld).

Any kernel that implements the MOSIX syscall ABI and filesystem hierarchy can
run MOSIX userspace programs. **Zirvium is the reference kernel** — a complete,
functional implementation you can build, boot, and fork today.

---

## MOSIX Filesystem Hierarchy

MOSIX replaces the traditional POSIX/FHS layout with a simpler structure:

| Path | Purpose | Replaces (POSIX) |
|------|---------|-------------------|
| `/bin` | Executable binaries | /bin, /sbin, /usr/bin, /usr/sbin |
| `/lib` | Libraries and headers | /lib, /usr/lib |
| `/user` | User home directories | /home, /root |
| `/boot` | Bootloader and kernel images | /boot |
| `/config` | System configuration | /etc |
| `/zirv` | Device namespace | /dev, /sys, /proc, /run |
| `/mounts` | Mount points | /mnt, /media |
| `/tmp` | Temporary files | /tmp |

---

## /zirv Device Namespace

```
/zirv/<bus>/<type><index>
```

| Bus | Media types |
|-----|-------------|
| `sata` | `hdd`, `ssd`, `cdrom` |
| `pata` | `hdd`, `ssd`, `cdrom` |
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
  arch/x64/          Boot, GDT, IDT, CPU helpers, ISR stubs, syscall entry
  kernel/            Core kernel
    main.c           Boot orchestrator (20-step init)
    mm/              PMM (bitmap), VMM (4-level paging), heap
    proc/            Process management, usermode entry
    syscall/         24 syscall dispatch handlers
    irq/             8259A PIC + IRQ handler registration
    ipc/             Ring-buffer pipe implementation
    loader/          ELF loader, embedded binary table, init_bin.asm
    time/            CMOS RTC, TSC uptime, timezone
  fs/                MOSIX VFS tree, console device node
  drivers/
    serial/          16550 UART (COM1, 115200 8N1)
    vga/             VGA text-mode, pixel framebuffer console
    pci/             PCI bus scanner, Linux compat layer
    gpu/             Enveediya GPU (Linux compat, nouveau port)
    gpu/             Intel Arc GPU (Linux compat, A/B/C-series)
    gpu/             AMD Radeon GPU (Linux compat, GCN/RDNA)
    usb/             xHCI USB 3.0 (Linux compat, polling)
    thunderbolt/     Thunderbolt 3/4 & USB4 (Linux compat)
    net/             RTL8139 NIC (Linux compat), WiFi 7 (Linux compat)
    compat/          linux_compat.h API bridge (610 lines)
    zirv/
      input/ps2/     i8042 controller + PS/2 keyboard + Synaptics
      display/       Bochs VGA, VMware SVGA II, Intel i915 (Gen 9.5-14)
      audio/hda/     Intel High-Definition Audio
      wifi/rtl8723de Realtek RTL8723DE WiFi (PCIe, DMA rings, efuse, TX)
      bluetooth/     Realtek Bluetooth (UART HCI, firmware download)
      displayjet/    MAEM-encrypted display driver (ChaCha20, 11 syscalls)
  libs/zirvlibc/     Freestanding C library (submodule)
  zirvinit/          PID 1 init process (submodule)
  zirvshell/         Interactive shell (submodule)
  zirvutils/         System utilities (submodule)
  zirvflux/          Display framework library (submodule)
  zirvui/            Desktop compositor linking ZirvTK (submodule)
  zirvtk/            Rust desktop toolkit — clock, taskbar, launcher, glass UI (submodule)
  Makefile
```

---

## Embedded Binaries (17 total)

Userspace programs are compiled as statically linked, no-pie ELFs and embedded
into the kernel via `incbin`:

| Path | Source | Description |
|------|--------|-------------|
| `/bin/init` | zirvinit/ | PID 1 — launches shell |
| `/bin/shell` | zirvshell/ | Interactive shell (19 commands) |
| `/bin/hello` | zirvutils/ | Test program |
| `/bin/cat` | zirvutils/ | File reader |
| `/bin/sysinfo` | zirvutils/ | System information |
| `/bin/clear` | zirvutils/ | Clear screen |
| `/bin/echo` | zirvutils/ | Print arguments |
| `/bin/reboot` | zirvutils/ | System reboot |
| `/bin/shutdown` | zirvutils/ | System shutdown |
| `/bin/poweroff` | zirvutils/ | System power-off |
| `/bin/suspend` | zirvutils/ | System suspend (stub) |
| `/bin/ping` | zirvutils/ | ICMP echo / DNS resolution |
| `/bin/sleep` | zirvutils/ | Busy-wait sleep |
| `/bin/true` | zirvutils/ | Exit code 0 |
| `/bin/false` | zirvutils/ | Exit code 1 |
| `/bin/yes` | zirvutils/ | Print "y" forever |
| `/bin/uname` | zirvutils/ | System information |
| `/bin/hostname` | zirvutils/ | Get/set hostname |

Run from shell: `run /bin/<name>`

---

## Syscalls (24 total)

Numbers follow Linux x86-64 ABI where applicable, with MOSIX extensions
(13, 89-90, 96-102) and DisplayJet extensions (110-120).

| # | Name | Description |
|---|------|-------------|
| 0 | SYS_READ | Read from FD |
| 1 | SYS_WRITE | Write to FD |
| 2 | SYS_OPEN | Open path |
| 3 | SYS_CLOSE | Close FD |
| 9 | SYS_MMAP | Anonymous memory map |
| 11 | SYS_MUNMAP | Unmap memory |
| 12 | SYS_BRK | Set program break |
| 13 | SYS_UPTIME | Seconds since boot |
| 22 | SYS_PIPE | Create pipe |
| 39 | SYS_GETPID | Get PID |
| 59 | SYS_EXECVE | Execute binary |
| 60 | SYS_EXIT | Exit process |
| 61 | SYS_WAIT4 | Wait for child |
| 78 | SYS_GETDENTS | Read directory entries |
| 79 | SYS_GETCWD | Get CWD |
| 80 | SYS_CHDIR | Change directory |
| 89 | SYS_GETHOSTNAME | Get hostname |
| 90 | SYS_SETHOSTNAME | Set hostname |
| 96 | SYS_GETDATETIME | Read CMOS RTC datetime |
| 97 | SYS_SETDATETIME | Set CMOS RTC datetime |
| 98 | SYS_GETTZ | Get timezone offset |
| 99 | SYS_SETTZ | Set timezone offset |
| 100 | SYS_REBOOT | ACPI reset (0xCF9) |
| 101 | SYS_SHUTDOWN | QEMU/Bochs exit |
| 102 | SYS_DNS_LOOKUP | Resolve hostname via UDP/DNS |

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

# Run under QEMU (serial -> terminal, VGA -> SDL window)
make run

# Kernel only (no embedded userspace, -DVMZIRV)
make vmzirv

# Debug with GDB
make debug
```

---

## Boot Sequence

1. Multiboot2 header loaded by GRUB at 0x100000
2. 32-bit stub checks CPUID/long mode, builds identity page tables
3. Higher-half kernel mapped at 0xFFFFFFFF80000000
4. 64-bit long mode, GDT loaded, far-jump to kernel_main
5. 20-step init: serial -> GDT/IDT/PIC -> PMM -> VMM -> console -> VFS
   -> processes -> syscalls -> time -> PCI -> drivers -> keyboard
6. PID 1 created, zirvinit.elf loaded, entered via IRETQ to ring 3
7. Init execve's into shell -> interactive MOSIX shell

---

## Supported Hardware

| Category | Hardware |
|----------|----------|
| **Storage** | SATA (ATA PIO), NVMe, USB mass storage, VirtIO block (linux\_compat) |
| **Networking** | Intel e1000, VirtIO net (linux\_compat), RTL8139 (linux\_compat), Realtek RTL8723DE WiFi |
| **Display** | Bochs VGA, VMware SVGA II, Intel i915 UHD (Gen 9.5-14), Enveediya GPU (linux\_compat, nouveau port — RTX/B-series), Intel Arc GPU (linux\_compat, A/B/C-series), AMD Radeon GPU (linux\_compat, GCN/RDNA) |
| **Input** | PS/2 keyboard (i8042), Synaptics touchpad |
| **USB** | xHCI (USB 3.0/2.0, linux\_compat, polling) — device enumeration, pendrive registration |
| **Bus** | Thunderbolt 3/4, USB4 (linux\_compat, detection) |
| **Wireless** | WiFi 7 / 802.11be (linux\_compat, Intel BE200/BE201, MediaTek MT7927, Qualcomm QCNCM865, Broadcom BCM4389) |
| **Other** | HDA audio, Realtek Bluetooth, TPM 2.0, serial 16550 UART |

---

## Creating a MOSIX Distribution

Zirvium is the reference implementation, but MOSIX is an open standard.
You can fork this kernel, write your own, or create a distribution that
uses different userspace components — as long as you follow the
[MOSIX specification](https://github.com/gauthamnair2005/zirvworld).

See the [distribution guide](https://github.com/gauthamnair2005/zirvworld/blob/main/distributions.html)
for details.
