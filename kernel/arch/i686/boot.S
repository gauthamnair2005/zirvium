/* boot.S - GAS Syntax */


/* Multiboot2 header */
.section .multiboot
.align 8
multiboot_header_start:
    .long 0xe85250d6                /* magic number */
    .long 0                         /* architecture (i386) */
    .long multiboot_header_end - multiboot_header_start  /* header length */
    .long -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))  /* checksum */
    
    /* end tag */
    .short 0
    .short 0
    .long 8
multiboot_header_end:

/* Stack */
.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

/* Entry point */
.section .text
.extern kernel_main
.global _start

_start:
    cli
    mov $stack_top, %esp
    
    /* Clear screen with blue background */
    mov $0xB8000, %edi
    mov $(80*25), %ecx
    mov $0x1F20, %ax  /* White on blue */
clear_loop:
    stosw
    loop clear_loop
    
    /* Write "Zirvium" at top */
    mov $0xB8000, %edi
    mov $boot_msg, %esi
    mov $0x1F, %ah  /* White on blue */
write_loop:
    lodsb
    test %al, %al
    jz write_done
    stosw
    jmp write_loop
write_done:
    
    /* Push multiboot magic and info pointer */
    push %ebx
    push %eax
    
    call kernel_main
    
    /* Halt message if returned */
    mov $(0xB8000 + (80*24*2)), %edi
    mov $halt_msg, %esi
    mov $0x4F, %ah  /* White on red */
write_halt:
    lodsb
    test %al, %al
    jz halt_done
    stosw
    jmp write_halt
halt_done:
    
    cli
hang:
    hlt
    jmp hang

.section .rodata
boot_msg: .asciz "Zirvium OS i686 - Initializing..."
halt_msg: .asciz "System Halted"
