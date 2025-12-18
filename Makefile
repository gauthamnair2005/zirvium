CC = gcc
AS = nasm
LD = ld

CFLAGS = -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
         -fno-pic -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 \
         -Iinclude -O2
LDFLAGS = -nostdlib -z max-page-size=0x1000
ASFLAGS = -f elf64

KERNEL_SRC = $(shell find kernel -name "*.c")
KERNEL_ASM = $(shell find kernel -name "*.asm")
LIB_SRC = $(shell find lib -name "*.c")
KERNEL_OBJ = $(KERNEL_SRC:.c=.o) $(KERNEL_ASM:.asm=.o) $(LIB_SRC:.c=.o)

BOOT_SRC = boot/boot.c
BOOT_OBJ = boot/boot.o

all: zirvium.elf

zirvium.elf: $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) -T kernel/linker.ld -o $@ $^

bootx64.efi: $(BOOT_OBJ)
	$(CC) -shared -Wl,-dll -Wl,--subsystem,10 -e efi_main -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

clean:
	find . -name "*.o" -type f -delete
	rm -f zirvium.elf bootx64.efi

run: all
	qemu-system-x86_64 -kernel zirvium.elf -m 512M -serial stdio

run-uefi: all
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -kernel zirvium.elf -m 512M

.PHONY: all clean run
