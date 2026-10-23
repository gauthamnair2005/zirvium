# Zirvium 2.0

Zirvium is a 64-bit operating system written entirely in Rust and assembly.
There is no C, no 32-bit boot stage, no compatibility layer and no legacy libc.
Version 2 is a from-scratch rewrite of the earlier C kernel; the old tree is
preserved only in git history.

The system is built around two ideas borrowed from earlier research systems:

* **MOSIX** — a cluster is one pool of computing resources. Processes have a
  home node and a current node; they may migrate without changing identity.
* **DisplayJet** — the display is a first-class subsystem with a persistent
  surface, damage tracking and a narrow GPU boundary, not a collection of
  ad-hoc framebuffer writes.

This milestone boots on UEFI, takes ownership of the machine, brings up the
architecture, drivers, storage-less MOSIX cluster and a complete graphical
desktop. A terminal, file browser, system monitor, node view, settings and
about application are included.

## Quick start

Requirements:

* Rust stable with `x86_64-unknown-uefi` and `x86_64-unknown-none`
* `qemu-system-x86`, OVMF firmware, `mtools` and `xorriso`

```bash
rustup target add x86_64-unknown-uefi x86_64-unknown-none
rustup component add rust-src llvm-tools-preview

make            # build the PE32+ image, ESP and ISO
make run        # boot the desktop in a QEMU window
make run-headless
```

`make run` uses KVM when `/dev/kvm` is writable and falls back to TCG. Firmware
paths can be overridden, for example:

```bash
make run OVMF_CODE=/usr/share/OVMF/OVMF_CODE_4M.fd \
         OVMF_VARS=/usr/share/OVMF/OVMF_VARS_4M.fd
```

Other targets: `make build`, `make esp`, `make iso`, `make check`,
`make clippy`, `make fmt`, `make test`, `make clean`.

The host-side tests for the standard library run with:

```bash
make test
```

## Boot path

1. Firmware loads the PE32+ image and calls `efi_main`.
2. Serial and the kernel logger come up.
3. The memory map, GOP scanout, ACPI RSDP and image range are captured.
4. The kernel switches to a static 256 KiB stack.
5. `ExitBootServices` is called and firmware code is never used again.
6. The frame allocator and a 96 MiB boundary-tagged kernel heap are installed.
7. The architecture layer builds a 64 GiB identity map with 2 MiB pages,
   loads the GDT/TSS and IDT, remaps the PIC and programs the PIT at 250 Hz.
8. PCI is enumerated, the RTC is read and PS/2 is initialised with interrupts
   still disabled.
9. The scanout pages are marked uncached, the embedded font atlas is parsed
   and DisplayJet creates its back buffer.
10. MOSIX starts the local node, the `int 0x80` gate is self-tested, then
    interrupts are enabled and the desktop runs the event loop.

There is no real-mode trampoline: UEFI supplies the initial 64-bit
environment and the kernel never leaves long mode.

## Layout

```
kernel/             the kernel binary
  src/arch/         CPU, GDT/IDT, PIC/PIT, paging, serial, ports
  src/uefi/         firmware hand-off and GOP framebuffer capture
  src/mm/           physical frame allocator and kernel heap
  src/drivers/      PCI, PS/2, RTC
  src/displayjet/   display engine, rasteriser and font atlas
  src/mosix/        cluster/process model and virtual filesystem
  src/ui/           desktop, window manager, applications
  src/syscall.rs    ABI dispatch for the software interrupt gate
libs/zirvstdlib/    the Zirvium standard library (no_std)
assets/font.bin     generated anti-aliased DejaVu Sans/Bold atlas
tools/genfont.py    atlas generator (development-time only)
```

`zirvstdlib` is a separate repository included as a submodule. It provides
geometry, colour, math, formatting, ring buffers, spin locks, an ABI and the
other primitives shared by the kernel and future userspace.

## DisplayJet

DisplayJet owns a persistent 32-bit ARGB surface. Every operation is clipped
and records damage; the completed surface is then presented to the scanout.
Drawing is anti-aliased: rounded rectangles, circles, shadows, gradients,
beziers and text all use coverage masks rather than hard edges. Text comes
from a generated atlas with 14 faces (regular and bold at seven sizes, 95
ASCII glyphs each) so the kernel does not depend on firmware fonts.

Rasterisation runs on the CPU with SSE2. The present path is deliberately the
only GPU-facing boundary: the firmware-programmed scanout today, with a
virtio-gpu resource/queue backend and Intel iGPU detection scaffolded in the
PCI and display layers. The desktop redraws only when structure, hover or
state changes; pointer motion moves the saved cursor patch.

## MOSIX

The local node publishes CPU count, memory and a load average. Processes are
registered with a home node, a current node, a state and accounting data. The
API is cluster-shaped from the start: a future network driver only needs to
publish peer nodes and the scheduler can migrate processes. In this milestone
the local machine is the only online member, and the demo node view shows how
peers and migrations will appear.

## Applications

* **Terminal** — shell over the MOSIX/VFS view: `help`, `ls`, `cd`, `cat`,
  `tree`, `ps`, `nodes`, `mem`, `uptime`, `date`, `neofetch`, and more.
* **Files** — browse the in-memory virtual filesystem with a preview pane.
* **System Monitor** — CPU load, heap use, process count and uptime.
* **MOSIX Nodes** — peers, process placement and migration controls.
* **Settings** — wallpaper, accent colour, grid and seconds display.
* **About** — the design story behind MOSIX and DisplayJet.

The window manager supports focus, z-order, dragging, resizing, maximising,
minimising, double-click title bars, a dock with magnification, a launcher
(Super) and transient notifications.

## Status

Working: UEFI boot, memory management, interrupts and timer, PS/2 keyboard
and mouse, PCI enumeration, RTC, DisplayJet software rasterisation and the
full desktop, MOSIX local cluster and VFS, syscall gate, serial diagnostics.

Next: virtio-gpu 2D resource/queue backend (real GPU presentation on QEMU),
networking for multi-node MOSIX, user mode and address-space isolation, a
persistent filesystem and a native GPU command stream for hardware iGPUs.

## License

Zirvium is GPL-3.0-or-later. The generated font atlas is derived from DejaVu
Sans; see `assets/FONT-LICENSE.txt`.
