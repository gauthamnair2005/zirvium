BITS 64

extern kernel_main
global kernel_entry

section .text.entry
kernel_entry:
    cli
    
    mov rsp, stack_top
    
    call kernel_main
    
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
