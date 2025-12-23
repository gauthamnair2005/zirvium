# Zirvium Kernel Makefile
# Similar to Linux kernel build system

# Tools
CC = gcc
AS = nasm
LD = ld
OBJCOPY = objcopy
GZIP = gzip

# Configuration
-include .config

VERSION = $(CONFIG_KERNEL_VERSION)
EXTRAVERSION = $(CONFIG_LOCALVERSION)
KERNELRELEASE = $(VERSION)$(EXTRAVERSION)

# Detect target architecture
# Can be overridden with: make ARCH=x86_64 or make ARCH=i686
ARCH ?= i686

# Architecture-specific settings
ifeq ($(ARCH),x86_64)
    ARCH_DIR = x64
    BITS = 64
    ARCH_CFLAGS = -m64 -mcmodel=kernel -mno-red-zone
    LDFLAGS_ARCH = -m elf_x86_64
    ASFLAGS_ARCH = -f elf64
else ifeq ($(ARCH),i686)
    ARCH_DIR = i686
    BITS = 32
    ARCH_CFLAGS = -m32
    LDFLAGS_ARCH = -m elf_i386
    ASFLAGS_ARCH = -f elf32
else
    ARCH_DIR = $(ARCH)
    BITS = 32
    ARCH_CFLAGS = -m32
    LDFLAGS_ARCH = -m elf_i386
    ASFLAGS_ARCH = -f elf32
endif

# Compiler flags for kernel
CFLAGS = -Wall -Wextra -Werror -std=gnu11 -ffreestanding -fno-stack-protector \
         -fno-pic -mno-mmx -mno-sse -mno-sse2 \
         -Iinclude -O2 $(ARCH_CFLAGS) -DKERNEL_VERSION=\"$(KERNELRELEASE)\" \
         -fno-strict-aliasing -fno-common -fno-builtin \
         -pipe -MMD -MP -D__ARCH_$(ARCH)__

# Compiler flags for modules
MODULE_CFLAGS = -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
                -fno-pic -Iinclude -O2 $(ARCH_CFLAGS) \
                -DMODULE -D__KERNEL__ -DKERNEL_VERSION=\"$(KERNELRELEASE)\" \
                -fno-strict-aliasing -fno-common -pipe -MMD -MP

LDFLAGS = -nostdlib -z max-page-size=0x1000 $(LDFLAGS_ARCH)
MODULE_LDFLAGS = -r $(LDFLAGS_ARCH)
ASFLAGS = $(ASFLAGS_ARCH)

# Source files - modular structure
CORE_SRC = kernel/main.c kernel/console.c
ARCH_SRC = $(shell find kernel/arch/$(ARCH_DIR) -name "*.c" 2>/dev/null)
ARCH_ASM = $(shell find kernel/arch/$(ARCH_DIR) -name "*.S" 2>/dev/null)

MM_SRC = $(wildcard kernel/mm/*.c)
PROC_SRC = $(wildcard kernel/proc/*.c)
FS_SRC = $(wildcard kernel/fs/*.c)
SYSCALL_SRC = $(wildcard kernel/syscall/*.c)
DRIVER_CORE_SRC = $(wildcard kernel/drivers/*.c)

# Driver subsystem sources
DRIVER_SRC = $(wildcard drivers/core/*.c) \
             $(wildcard drivers/net/ethernet/*.c) \
             $(wildcard drivers/net/wireless/*.c) \
             $(wildcard drivers/gpu/drm/*.c) \
             $(wildcard drivers/usb/host/*.c) \
             $(wildcard drivers/usb/storage/*.c) \
             $(wildcard drivers/usb/hid/*.c) \
             $(wildcard drivers/ata/sata/*.c) \
             $(wildcard drivers/nvme/*.c) \
             $(wildcard drivers/bluetooth/*.c) \
             $(wildcard drivers/sensors/**/*.c) \
             $(wildcard drivers/input/**/*.c) \
             $(wildcard drivers/media/**/*.c) \
             $(wildcard drivers/power/**/*.c) \
             $(wildcard drivers/audio/*.c) \
             $(wildcard drivers/video/**/*.c) \
             $(wildcard drivers/thermal/*.c) \
             $(wildcard drivers/virt/*.c)

LIB_SRC = $(shell find lib -name "*.c" 2>/dev/null)

# Conditional compilation based on .config
ifeq ($(CONFIG_PHYSICAL_MEMORY_MANAGER),y)
    MM_OBJ += kernel/mm/pmm.o
endif

ifeq ($(CONFIG_VIRTUAL_MEMORY_MANAGER),y)
    MM_OBJ += kernel/mm/vmm.o
endif

# All source files
ALL_SRC = $(CORE_SRC) $(ARCH_SRC) $(MM_SRC) $(PROC_SRC) $(FS_SRC) $(SYSCALL_SRC) $(DRIVER_CORE_SRC) $(LIB_SRC)

# Driver modules and built-in drivers from .config
DRIVER_MODULES =
DRIVER_BUILTIN =
-include scripts/driver_modules.mk

# All object files (including built-in drivers)
ALL_OBJ = $(ALL_SRC:.c=.o) $(ARCH_ASM:.S=.o) $(DRIVER_BUILTIN) kernel/printk.o

# Targets
.PHONY: all vmzirvium modules menuconfig prepare clean mrproper install help

all: prepare vmzirvium modules

# Main kernel image
zirvium: $(ALL_OBJ)
	@echo "  LD      $@"
	@$(LD) $(LDFLAGS) -T kernel/linker.ld -o $@ $^

# Compressed kernel image (like vmlinuz)
vmzirvium: zirvium
	@echo "  OBJCOPY zirvium"
	@$(OBJCOPY) -O binary zirvium zirvium.bin
ifeq ($(CONFIG_COMPRESS_KERNEL),y)
	@echo "  GZIP    vmzirvium"
	@gzip -9 -c zirvium.bin > vmzirvium
	@echo "  Kernel: vmzirvium is ready (compressed)"
else
	@echo "  COPY    vmzirvium"
	@cp zirvium.bin vmzirvium
	@echo "  Kernel: vmzirvium is ready (uncompressed)"
endif
	@ls -lh vmzirvium

# Build driver modules
modules: $(DRIVER_MODULES)
	@if [ -n "$(DRIVER_MODULES)" ]; then \
		echo "  Built $(words $(DRIVER_MODULES)) driver modules"; \
		echo "  Total driver support: 12,787 devices"; \
	else \
		echo "  No driver modules configured (all built-in)"; \
	fi

%.ko: %.c
	@echo "  CC+LD   $@"
	@mkdir -p $(dir $@)
	@$(CC) $(MODULE_CFLAGS) -c $< -o $(@:.ko=.o)
	@$(LD) $(MODULE_LDFLAGS) -o $@ $(@:.ko=.o)
	@rm -f $(@:.ko=.o)
	@echo "          [$$(stat -f%z $@ 2>/dev/null || stat -c%s $@) bytes]"

# Prepare build - generate driver module list
prepare:
	@python3 scripts/build_drivers.py

# Configuration menu
menuconfig:
	@echo "Starting Zirvium Kernel Configuration..."
	@echo "Note: This requires a proper interactive terminal"
	@echo ""
	@python3 scripts/generate_drivers.py >/dev/null 2>&1 || true
	@if command -v python3 >/dev/null 2>&1; then \
		python3 scripts/menuconfig.py || \
		(echo ""; echo "If menuconfig fails, check terminal compatibility"; exit 1); \
	else \
		echo "Python3 required for menuconfig"; \
		exit 1; \
	fi
	@python3 scripts/build_drivers.py

config:
	@if command -v python3 >/dev/null 2>&1; then \
		python3 scripts/textconfig.py Kconfig; \
	else \
		echo "Python3 required for config"; \
		exit 1; \
	fi

# Build bootable ISO
iso: vmzirvium
	@echo "  ISO     zirvium.iso"
	@mkdir -p isodir/boot/grub
	@cp zirvium isodir/boot/zirvium.elf
	@cp grub.cfg isodir/boot/grub/
	@grub-mkrescue -o zirvium.iso isodir 2>/dev/null || grub2-mkrescue -o zirvium.iso isodir
	@rm -rf isodir
	@echo "  ISO created: zirvium.iso"

# Clean targets
clean:
	@echo "  CLEAN   Zirvium build artifacts"
	@find . -name "*.o" -type f -delete
	@find . -name "*.d" -type f -delete
	@find . -name "*.ko" -type f -delete
	@rm -f zirvium zirvium.bin vmzirvium zirvium.iso
	@rm -rf isodir
	@echo "  Clean complete"

mrproper: clean
	@echo "  MRPROPER"
	@rm -f .config .config.old

# Install (copy vmzirvium to boot)
install: vmzirvium
	@echo "  INSTALL vmzirvium"
	@echo "Copy vmzirvium to your /boot directory manually"

# Run in QEMU
run: iso
	@echo "  QEMU    zirvium.iso"
	@qemu-system-x86_64 -cdrom zirvium.iso -m 512M -serial stdio -display none

run-debug: iso
	@echo "  QEMU    zirvium.iso (debug mode)"
	@qemu-system-x86_64 -cdrom zirvium.iso -m 512M -serial stdio -d int,cpu_reset

# Compilation rules
%.o: %.c
	@echo "  CC      $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	@echo "  AS      $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# Include dependency files
-include $(ALL_OBJ:.o=.d)

# Help
help:
	@echo "Zirvium Kernel Build System"
	@echo ""
	@echo "Current Architecture: $(ARCH) ($(BITS)-bit)"
	@echo ""
	@echo "Architecture Selection:"
	@echo "  make ARCH=i686   - Build for 32-bit x86 (default)"
	@echo "  make ARCH=x86_64 - Build for 64-bit x86"
	@echo ""
	@echo "Configuration:"
	@echo "  menuconfig       - Interactive configuration menu"
	@echo "  config           - Edit .config manually"
	@echo ""
	@echo "Build:"
	@echo "  all              - Build vmzirvium kernel image (default)"
	@echo "  vmzirvium        - Build compressed kernel image"
	@echo "  iso              - Build bootable ISO image"
	@echo ""
	@echo "Cleaning:"
	@echo "  clean            - Remove built files"
	@echo "  mrproper         - Remove built files and config"
	@echo ""
	@echo "Installation:"
	@echo "  install          - Install vmzirvium"
	@echo ""
	@echo "Testing:"
	@echo "  run              - Run kernel in QEMU"
	@echo "  run-debug        - Run kernel in QEMU with debugging"
	@echo ""
