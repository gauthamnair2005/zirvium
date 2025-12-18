BITS 32

section .multiboot
align 8
multiboot_header_start:
    dd 0xe85250d6                ; magic number
    dd 0                         ; architecture (i386)
    dd multiboot_header_end - multiboot_header_start  ; header length
    dd -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))  ; checksum
    
    ; end tag
    dw 0
    dw 0
    dd 8
multiboot_header_end:

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
extern kernel_main
global _start

_start:
    cli
    mov esp, stack_top
    
    ; Clear screen and write boot message
    mov eax, 0xB8000
    mov ecx, 80*25
    mov word [eax], 0x0F20  ; Clear with white on black
.clear_loop:
    add eax, 2
    loop .clear_loop
    
    ; Write "Zirvium" at top
    mov edi, 0xB8000
    mov esi, boot_msg
    mov ah, 0x0F
.write_loop:
    lodsb
    test al, al
    jz .write_done
    stosw
    jmp .write_loop
.write_done:
    
    call kernel_main
    
    cli
.hang:
    hlt
    jmp .hang

section .rodata
boot_msg: db 'Zirvium OS Booting...', 0
