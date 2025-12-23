/* idt_flush.S */
.global idt_flush

idt_flush:
    mov 4(%esp), %eax    /* Get pointer to IDT */
    lidt (%eax)          /* Load IDT */
    ret
