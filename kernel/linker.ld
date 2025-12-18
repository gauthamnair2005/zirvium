ENTRY(kernel_entry)

SECTIONS {
    . = 0xFFFFFFFF80100000;
    
    .text : {
        *(.text.entry)
        *(.text)
    }
    
    .rodata : {
        *(.rodata*)
    }
    
    .data : {
        *(.data)
    }
    
    .bss : {
        *(COMMON)
        *(.bss)
    }
}
