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
    
    ; Write 'Z' to screen to show we booted
    mov eax, 0xB8000
    mov byte [eax], 'Z'
    mov byte [eax+1], 0x0F
    
    call kernel_main
    
    cli
.hang:
    hlt
    jmp .hang
