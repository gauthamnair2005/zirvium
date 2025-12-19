BITS 32

global idt_flush

idt_flush:
    mov eax, [esp+4]    ; Get the pointer to the IDT passed as parameter
    lidt [eax]          ; Load the IDT
    ret
