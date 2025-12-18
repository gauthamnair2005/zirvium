CC = gcc
AS = nasm
LD = ld

CFLAGS = -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector \
         -fno-pic -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 \
         -Iinclude -O2
LDFLAGS = -nostdlib -z max-page-size=0x1000
ASFLAGS = -f elf64

KERNEL_SRC = $(wildcard kernel/*.c kernel/**/*.c)
KERNEL_ASM = $(wildcard kernel/*.asm kernel/**/*.asm)
KERNEL_OBJ = $(KERNEL_SRC:.c=.o) $(KERNEL_ASM:.asm=.o)

BOOT_SRC = boot/boot.c
BOOT_OBJ = boot/boot.o

all: zirvium.elf bootx64.efi

zirvium.elf: $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) -T kernel/linker.ld -o $@ $^

bootx64.efi: $(BOOT_OBJ)
	$(CC) -shared -Wl,-dll -Wl,--subsystem,10 -e efi_main -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -f $(KERNEL_OBJ) $(BOOT_OBJ) zirvium.elf bootx64.efi

run: all
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -kernel zirvium.elf -m 512M

.PHONY: all clean run
