# Zirvium Kernel — Top-level Makefile
#
# Targets:
#   make          — build the kernel ELF
#   make iso      — build a bootable ISO image (requires grub-mkrescue, xorriso & mtools)
#   make run      — build ISO and run in QEMU via GRUB (-cdrom)
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
    -I.

ASFLAGS := -f elf64

LDFLAGS := \
    -nostdlib               \
    -T kernel/kernel.ld     \
    -z max-page-size=0x1000

# ── Sources ────────────────────────────────────────────────────────────────────
C_SRCS := \
    kernel/main.c                                   \
    kernel/console.c                                \
    kernel/mm/pmm.c                                 \
    kernel/mm/vmm.c                                 \
    kernel/mm/heap.c                                \
    kernel/irq/irq.c                                \
    kernel/proc/process.c                           \
    kernel/syscall/syscall.c                        \
    kernel/ipc/pipe.c                               \
    arch/x64/gdt.c                                  \
    arch/x64/idt.c                                  \
    fs/vfs.c                                        \
    drivers/pci/pci.c                               \
    drivers/vga/vga.c                               \
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
    drivers/serial/serial.c                         \
    lib/string.c                                    \
    lib/stdio.c                                     \
    lib/ctype.c

ASM_SRCS := \
    arch/x64/boot.asm           \
    arch/x64/isr_stubs.asm      \
    arch/x64/syscall_entry.asm

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

# ── Strip to flat binary (optional) ───────────────────────────────────────────
.PHONY: bin
bin: $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $(KERNEL_BIN)
	@echo "  BIN $(KERNEL_BIN)"

# ── Build a GRUB2 Multiboot2 ISO ──────────────────────────────────────────────
ISO_DIR   := $(BUILD_DIR)/iso
GRUB_CFG  := $(ISO_DIR)/boot/grub/grub.cfg
KERNEL_ISO := $(BUILD_DIR)/zirvium.iso

.PHONY: iso
iso: $(KERNEL_ELF)
	@command -v mformat >/dev/null 2>&1 || \
	    { echo "Error: 'mformat' not found. Install mtools: sudo apt install mtools"; exit 1; }
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_ELF) $(ISO_DIR)/boot/zirvium.elf
	@printf 'set timeout=0\nset default=0\n\nmenuentry "Zirvium" {\n  multiboot2 /boot/zirvium.elf\n  boot\n}\n' \
	    > $(GRUB_CFG)
	grub-mkrescue -o $(KERNEL_ISO) $(ISO_DIR)
	@echo "  ISO $(KERNEL_ISO)"

# ── Run in QEMU ────────────────────────────────────────────────────────────────
# QEMU's -kernel flag requires a Linux bzImage or PVH ELF; it does not support
# Multiboot2.  Boot via GRUB using the ISO image instead.
#
# -vga std   — standard VGA adapter with Bochs VBE extensions (PCI 0x1234:0x1111)
# -display sdl — open a window so boot messages appear on the VGA console
# -serial stdio — mirror serial output to the host terminal for debugging
.PHONY: run
run: $(KERNEL_ISO)
	qemu-system-x86_64              \
	    -cdrom $(KERNEL_ISO)        \
	    -boot d                     \
	    -vga std                    \
	    -display sdl                \
	    -serial stdio               \
	    -m 256M                     \
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

# ── Clean ─────────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "  CLEAN"
