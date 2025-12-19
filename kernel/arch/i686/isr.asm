BITS 32

global isr_stub_0

; Default interrupt handler - just returns
isr_stub_0:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp
    
    ; Call C interrupt handler if needed
    ; extern isr_handler
    ; call isr_handler
    
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    iret
