/* gdt_flush.S */
.global gdt_flush

gdt_flush:
    mov 4(%esp), %eax    /* Get pointer to GDT */
    lgdt (%eax)          /* Load GDT */
    
    mov $0x10, %ax       /* Data segment offset */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    
    ljmp $0x08, $flush   /* Far jump to code segment */
flush:
    ret
