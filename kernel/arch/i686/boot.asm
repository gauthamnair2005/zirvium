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
    
    ; Clear screen with blue background
    mov edi, 0xB8000
    mov ecx, 80*25
    mov ax, 0x1F20  ; White on blue (0x1F = blue background, white foreground)
.clear_loop:
    stosw
    loop .clear_loop
    
    ; Write "Zirvium" at top
    mov edi, 0xB8000
    mov esi, boot_msg
    mov ah, 0x1F  ; White on blue
.write_loop:
    lodsb
    test al, al
    jz .write_done
    stosw
    jmp .write_loop
.write_done:
    
    ; Push multiboot magic and info pointer if available
    ; EAX should contain multiboot magic from bootloader
    ; EBX should contain multiboot info structure address
    push ebx
    push eax
    
    call kernel_main
    
    ; If we return, show halt message
    mov edi, 0xB8000 + (80*24*2)  ; Bottom line
    mov esi, halt_msg
    mov ah, 0x4F  ; White on red
.write_halt:
    lodsb
    test al, al
    jz .halt_done
    stosw
    jmp .write_halt
.halt_done:
    
    cli
.hang:
    hlt
    jmp .hang

section .rodata
boot_msg: db 'Zirvium OS i686 - Initializing...', 0
halt_msg: db 'System Halted', 0
