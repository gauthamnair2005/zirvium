# Zirvium Kernel Makefile
# Similar to Linux kernel build system

# Tools
CC = gcc
AS = nasm
LD = ld
OBJCOPY = objcopy

# Configuration
-include .config

VERSION = $(CONFIG_KERNEL_VERSION)
EXTRAVERSION = $(CONFIG_LOCALVERSION)
KERNELRELEASE = $(VERSION)$(EXTRAVERSION)

# Compiler flags
CFLAGS = -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
         -fno-pic -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -Iinclude -O2 -m32 -DKERNEL_VERSION=\"$(KERNELRELEASE)\"

LDFLAGS = -nostdlib -z max-page-size=0x1000 -m elf_i386
ASFLAGS = -f elf32

# Source files - modular structure
CORE_SRC = kernel/main.c kernel/console.c
ARCH_SRC = $(shell find kernel/arch/x64 -name "*.c" 2>/dev/null)
ARCH_ASM = $(shell find kernel/arch/x64 -name "*.asm" 2>/dev/null)

MM_SRC = $(wildcard kernel/mm/*.c)
PROC_SRC = $(wildcard kernel/proc/*.c)
FS_SRC = $(wildcard kernel/fs/*.c)
SYSCALL_SRC = $(wildcard kernel/syscall/*.c)
DRIVERS_SRC = $(wildcard kernel/drivers/*.c)

LIB_SRC = $(shell find lib -name "*.c" 2>/dev/null)

# Conditional compilation based on .config
ifeq ($(CONFIG_PHYSICAL_MEMORY_MANAGER),y)
    MM_OBJ += kernel/mm/pmm.o
endif

ifeq ($(CONFIG_VIRTUAL_MEMORY_MANAGER),y)
    MM_OBJ += kernel/mm/vmm.o
endif

# All source files
ALL_SRC = $(CORE_SRC) $(ARCH_SRC) $(MM_SRC) $(PROC_SRC) $(FS_SRC) $(SYSCALL_SRC) $(DRIVERS_SRC) $(LIB_SRC)
ALL_OBJ = $(ALL_SRC:.c=.o) $(ARCH_ASM:.asm=.o)

# Targets
.PHONY: all vmzirvium menuconfig clean mrproper install help

all: vmzirvium

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

# Configuration menu
menuconfig:
	@if command -v python3 >/dev/null 2>&1; then \
		python3 scripts/menuconfig.py; \
	else \
		./scripts/menuconfig.sh; \
	fi

config:
	@echo "Edit .config manually or run 'make menuconfig'"

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
	@echo "  CLEAN"
	@find . -name "*.o" -type f -delete
	@rm -f zirvium zirvium.bin vmzirvium zirvium.iso
	@rm -rf isodir

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
	@qemu-system-x86_64 -cdrom zirvium.iso -m 512M -serial stdio

run-debug: iso
	@echo "  QEMU    zirvium.iso (debug mode)"
	@qemu-system-x86_64 -cdrom zirvium.iso -m 512M -serial stdio -d int,cpu_reset

# Compilation rules
%.o: %.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	@echo "  AS      $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Help
help:
	@echo "Zirvium Kernel Build System"
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
