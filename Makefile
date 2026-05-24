# Zirvium Kernel — Top-level Makefile
#
# Targets:
#   make          — build the kernel ELF (with embedded user-space binaries)
#   make iso      — build a bootable ISO image (requires grub-mkrescue, xorriso & mtools)
#   make run      — build ISO and run in QEMU via GRUB (-cdrom)
#   make vmzirv   — build kernel-only ELF (no embedded binaries) for distro use
#   make clean    — remove build artefacts
#
# Requirements:
#   x86_64-elf-gcc   (or gcc with multilib / cross-compile configured)
#   x86_64-elf-ld    (or ld)
#   nasm             (>= 2.14)
#   grub-mkrescue    (for `make iso` / `make run`)
#   xorriso          (for `make iso` / `make run`)
#   mtools           (for `make iso` / `make run` — install with: sudo apt install mtools)

# ── Toolchain ─────────────────────────────────────────────────────────────────
CC      := x86_64-elf-gcc
LD      := x86_64-elf-ld
AS      := nasm
OBJCOPY := x86_64-elf-objcopy

# Fall back to the system gcc if the cross-compiler is not installed
ifeq (, $(shell which $(CC) 2>/dev/null))
  CC      := gcc
  LD      := ld
  OBJCOPY := objcopy
endif

# ── Flags ─────────────────────────────────────────────────────────────────────
CFLAGS := \
    -std=c11                \
    -ffreestanding          \
    -fno-stack-protector    \
    -fno-pic                \
    -mno-red-zone           \
    -mno-mmx                \
    -mno-sse                \
    -mno-sse2               \
    -mcmodel=kernel         \
    -D_FORTIFY_SOURCE=0     \
    -Wall                   \
    -Wextra                 \
    -O2                     \
    -fcf-protection=none    \
    -DKERNEL                \
    -I. \
    -Ilibs/zirvlibc/include \
    -Izirvdisplayjet \
    -Izirvdisplayjet/drivers/zirv/displayjet \
    -Izirvflux/include

ASFLAGS := -f elf64

LDFLAGS := \
    -nostdlib               \
    -T kernel/kernel.ld     \
    -z max-page-size=0x1000

# ── Sources ────────────────────────────────────────────────────────────────────
C_SRCS := \
    kernel/main.c                                   \
    kernel/console.c                                \
    kernel/time/time.c                              \
    kernel/mm/pmm.c                                 \
    kernel/mm/vmm.c                                 \
    kernel/mm/heap.c                                \
    kernel/loader/elf.c                             \
    kernel/loader/embedded.c                        \
    kernel/irq/irq.c                                \
    kernel/proc/process.c                           \
    kernel/proc/scheduler.c                         \
    kernel/syscall/syscall.c                        \
    kernel/audio/audio.c                            \
    kernel/ipc/pipe.c                               \
    kernel/ipc/mqueue.c                             \
    kernel/hpc/hpc.c                                \
    arch/x64/gdt.c                                  \
    arch/x64/idt.c                                  \
    fs/vfs.c                                        \
    fs/tmpfs.c                                      \
    drivers/pci/pci.c                               \
    drivers/vga/vga.c                               \
    drivers/vga/fb_console.c                        \
    drivers/zirv/device.c                           \
    drivers/zirv/sata.c                             \
    drivers/zirv/nvme.c                             \
    drivers/zirv/usb_storage.c                      \
    drivers/zirv/input/ps2/i8042.c                  \
    drivers/zirv/input/ps2/keyboard.c               \
    drivers/zirv/input/ps2/synaptics.c              \
    drivers/zirv/wifi/rtl8723de/rtl8723de.c         \
    drivers/zirv/bluetooth/btrtl.c                  \
    drivers/zirv/display/bochs/bochs_vga.c          \
    drivers/zirv/display/i915/i915.c                \
    drivers/zirv/audio/hda/hda.c                    \
    drivers/zirv/audio/ac97/ac97.c                  \
    drivers/serial/serial.c                         \
    drivers/zirv/driver.c                           \
    drivers/zirv/display/vmware.c                   \
    drivers/zirv/tpm.c                              \
    kernel/net/stack.c                              \
    drivers/zirv/intel_e1000.c                      \
    drivers/zirv/virtio.c                           \
    drivers/virtio/virtio_ring.c                    \
    drivers/virtio/virtio_blk.c                     \
    drivers/virtio/virtio_net.c                     \
    drivers/net/rtl8139/rtl8139.c                   \
    drivers/pci/pci_compat.c                        \
    drivers/gpu/enveediya/enveediya.c               \
    drivers/gpu/arc/arc.c                           \
    drivers/gpu/radeon/radeon.c                     \
    drivers/usb/xhci/xhci.c                         \
    drivers/thunderbolt/tb.c                        \
    drivers/net/wifi7/wifi7.c                       \
    zirvdisplayjet/drivers/zirv/displayjet/displayjet.c \
    zirvdisplayjet/drivers/zirv/displayjet/crypto.c \
    libs/zirvlibc/src/string.c                      \
    libs/zirvlibc/src/stdio.c                       \
    libs/zirvlibc/src/ctype.c

ASM_SRCS := \
    arch/x64/boot.asm           \
    arch/x64/isr_stubs.asm      \
    arch/x64/syscall_entry.asm  \
    kernel/loader/init_bin.asm

ZIRVINIT_ELF  := zirvinit/zirvinit.elf
ZIRVSHELL_ELF := zirvshell/zirvshell.elf
ZIRVUTILS_ELFS := zirvutils/hello.elf zirvutils/cat.elf zirvutils/sysinfo.elf zirvutils/clear.elf zirvutils/echo.elf zirvutils/reboot.elf zirvutils/shutdown.elf zirvutils/suspend.elf zirvutils/poweroff.elf zirvutils/ping.elf zirvutils/sleep.elf zirvutils/true.elf zirvutils/false.elf zirvutils/yes.elf zirvutils/uname.elf zirvutils/hostname.elf zirvutils/ifconfig.elf zirvutils/lspci.elf zirvutils/nokia.elf
ZIRVUI_ELF := zirvui/zirvui.elf

ZIRVFLUX_LIB := zirvflux/libzirvflux.a

# ── Derived object lists ───────────────────────────────────────────────────────
BUILD_DIR := build

C_OBJS   := $(patsubst %.c,   $(BUILD_DIR)/%.o, $(C_SRCS))
ASM_OBJS := $(patsubst %.asm, $(BUILD_DIR)/%.asm.o, $(ASM_SRCS))
ALL_OBJS := $(ASM_OBJS) $(C_OBJS)

KERNEL_ELF := $(BUILD_DIR)/zirvium.elf
KERNEL_BIN := $(BUILD_DIR)/zirvium.bin

# ── Default target ─────────────────────────────────────────────────────────────
.PHONY: all
all: $(KERNEL_ELF)

# ── Link ──────────────────────────────────────────────────────────────────────
$(KERNEL_ELF): $(ALL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "  LD  $@"

# ── Compile C ─────────────────────────────────────────────────────────────────
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<
	@echo "  CC  $<"

# ── Assemble ──────────────────────────────────────────────────────────────────
$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) -o $@ $<
	@echo "  AS  $<"

ifneq ($(filter vmzirv,$(MAKECMDGOALS)),)
# vmzirv build — init_bin.asm is assembled with -DVMZIRV which produces
# empty stubs instead of incbin, so no user-space ELF dependency needed.
$(BUILD_DIR)/kernel/loader/init_bin.asm.o: kernel/loader/init_bin.asm
else
$(BUILD_DIR)/kernel/loader/init_bin.asm.o: kernel/loader/init_bin.asm $(ZIRVINIT_ELF) $(ZIRVSHELL_ELF) $(ZIRVUTILS_ELFS) $(ZIRVUI_ELF)
endif

$(ZIRVINIT_ELF):
	$(MAKE) -C zirvinit

$(ZIRVSHELL_ELF):
	$(MAKE) -C zirvshell

# Build all utilities with a single sub-make invocation (avoids -j races)
$(ZIRVUTILS_ELFS): $(BUILD_DIR)/.zirvutils_stamp
$(BUILD_DIR)/.zirvutils_stamp:
	$(MAKE) -C zirvutils
	@touch $@

$(ZIRVFLUX_LIB):
	$(MAKE) -C zirvflux

$(ZIRVUI_ELF): $(ZIRVFLUX_LIB)
	$(MAKE) -C zirvui

# ── Strip to flat binary (optional) ───────────────────────────────────────────
.PHONY: bin
bin: $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $(KERNEL_BIN)
	@echo "  BIN $(KERNEL_BIN)"

# ── Build a GRUB2 Multiboot2 ISO ──────────────────────────────────────────────
ISO_DIR   := $(BUILD_DIR)/iso
GRUB_CFG  := $(ISO_DIR)/boot/grub/grub.cfg
KERNEL_ISO := $(BUILD_DIR)/zirvium.iso

$(KERNEL_ISO): $(KERNEL_ELF)
	@command -v mformat >/dev/null 2>&1 || \
	    { echo "Error: 'mformat' not found. Install mtools: sudo apt install mtools"; exit 1; }
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/zirvium.elf
	@printf 'set timeout=0\nset default=0\n\nmenuentry "Zirvium" {\n  multiboot2 /boot/zirvium.elf\n  boot\n}\n' \
	    > $(GRUB_CFG)
	grub-mkrescue -o $(KERNEL_ISO) $(ISO_DIR)
	@echo "  ISO $(KERNEL_ISO)"

.PHONY: iso
iso: $(KERNEL_ISO)

# ── Run in QEMU ────────────────────────────────────────────────────────────────
# QEMU's -kernel flag requires a Linux bzImage or PVH ELF; it does not support
# Multiboot2.  Boot via GRUB using the ISO image instead.
#
# -vga std   — standard VGA adapter with Bochs VBE extensions (PCI 0x1234:0x1111)
# -display sdl — open a window so boot messages appear on the VGA console
# -serial stdio — mirror serial output to the host terminal for debugging
.PHONY: run
run: $(KERNEL_ISO)
	@mkdir -p $(BUILD_DIR)
	@if [ ! -f $(BUILD_DIR)/blank.img ]; then dd if=/dev/zero of=$(BUILD_DIR)/blank.img bs=1M count=10; fi
	qemu-system-x86_64              \
	    -cdrom $(KERNEL_ISO)        \
	    -drive file=$(BUILD_DIR)/blank.img,format=raw,if=ide \
	    -drive file=/dev/zero,format=raw,if=none,id=vdisk0 -device virtio-blk-pci,drive=vdisk0 \
	    -netdev user,id=net0 -device e1000,netdev=net0      \
	    -netdev user,id=net1 -device virtio-net-pci,netdev=net1 \
	    -netdev user,id=net2 -device rtl8139,netdev=net2    \
	    -boot d                     \
	    -vga std                    \
	    -display sdl                \
	    -audio pa,model=ac97 \
	    -serial stdio               \
	    -m 512M                     \
	    -no-reboot

# ── Run with GDB attached ─────────────────────────────────────────────────────
.PHONY: debug
debug: $(KERNEL_ISO)
	qemu-system-x86_64              \
	    -cdrom $(KERNEL_ISO)        \
	    -boot d                     \
	    -vga std                    \
	    -display sdl                \
	    -serial stdio               \
	    -m 256M                     \
	    -s -S &
	gdb $(KERNEL_ELF)               \
	    -ex "target remote :1234"

# ── vmzirv — Kernel-only ELF (no embedded user-space binaries) ─────────────────
# Produces build/vmzirv, a pure kernel ELF at the normal link address.
# Distro makers can use this and link in their own initramfs/userspace manually.
# Existing targets (make, make iso, make run) are completely unaffected.
.PHONY: vmzirv
vmzirv: CFLAGS += -DVMZIRV
vmzirv: ASFLAGS += -DVMZIRV
vmzirv: $(BUILD_DIR)/vmzirv

$(BUILD_DIR)/vmzirv: $(ALL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "  LD  vmzirv $@"

# ── Clean ─────────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C zirvutils clean || true
	$(MAKE) -C zirvinit clean || true
	$(MAKE) -C zirvshell clean || true
	$(MAKE) -C zirvutils clean || true
	$(MAKE) -C zirvui clean || true
	$(MAKE) -C zirvflux clean || true
	@echo "  CLEAN"
