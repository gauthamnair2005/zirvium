/* isr.S */
.global isr_stub_0

isr_stub_0:
    push %eax
    push %ebx
    push %ecx
    push %edx
    push %esi
    push %edi
    push %ebp
    
    /* call isr_handler if needed */
    
    pop %ebp
    pop %edi
    pop %esi
    pop %edx
    pop %ecx
    pop %ebx
    pop %eax
    iret
