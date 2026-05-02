; arch/x64/boot.asm
; Zirvium Kernel - x86-64 Multiboot2 Boot Entry
; Follows the Multiboot2 specification (GNU GRUB compatible)

bits 32                     ; Multiboot2 starts in 32-bit protected mode

; ─── Multiboot2 Header ────────────────────────────────────────────────────────
MULTIBOOT2_MAGIC    equ 0xE85250D6
MULTIBOOT2_ARCH     equ 0           ; i386 protected mode
MULTIBOOT2_LENGTH   equ (mb2_header_end - mb2_header_start)
MULTIBOOT2_CHECKSUM equ -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + MULTIBOOT2_LENGTH)

section .multiboot2
align 8
mb2_header_start:
    dd MULTIBOOT2_MAGIC
    dd MULTIBOOT2_ARCH
    dd MULTIBOOT2_LENGTH
    dd MULTIBOOT2_CHECKSUM

    ; Framebuffer tag (request linear framebuffer)
    align 8
    dw 5                    ; type = framebuffer
    dw 1                    ; flags = optional
    dd 20                   ; size
    dd 0                    ; width  (0 = let firmware decide)
    dd 0                    ; height (0 = let firmware decide)
    dd 32                   ; depth  (32 bpp)

    ; End tag
    align 8
    dw 0
    dw 0
    dd 8
mb2_header_end:

; ─── Kernel Stack ─────────────────────────────────────────────────────────────
section .bss
align 16
stack_bottom:
    resb 32768              ; 32 KiB initial kernel stack
stack_top:

; ─── Page Tables (identity map first 4 GiB) ───────────────────────────────────
align 4096
pml4_table:  resb 4096
pdp_table:   resb 4096
pd_table:    resb 4096 * 4  ; 4 PDPT entries → 4 × 512 × 2 MiB = 4 GiB

; ─── GDT for long mode ────────────────────────────────────────────────────────
section .data
align 8
gdt64:
    dq 0x0000000000000000   ; null descriptor
.code: equ $ - gdt64
    dq 0x00AF9A000000FFFF   ; 64-bit code: L=1, P=1, DPL=0, S=1, type=0xA
.data: equ $ - gdt64
    dq 0x00CF92000000FFFF   ; 64-bit data: G=1, P=1, DPL=0, S=1, type=0x2
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

; ─── 32-bit Bootstrap Code ────────────────────────────────────────────────────
section .text
global _start
_start:
    ; Save Multiboot2 info pointer (ebx) and magic (eax)
    mov edi, eax            ; magic  → edi  (1st arg to kernel_main)
    mov esi, ebx            ; info*  → esi  (2nd arg to kernel_main)

    ; Check CPUID support
    pushfd
    pop  eax
    mov  ecx, eax
    xor  eax, 1 << 21
    push eax
    popfd
    pushfd
    pop  eax
    push ecx
    popfd
    xor  eax, ecx
    jz   .no_cpuid

    ; Check long mode support (CPUID 0x80000001)
    mov  eax, 0x80000000
    cpuid
    cmp  eax, 0x80000001
    jb   .no_long_mode
    mov  eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz   .no_long_mode

    ; ── Build identity page tables ──────────────────────────────────
    ; PML4[0] → PDP
    mov  eax, pdp_table
    or   eax, 0x03          ; present + writable
    mov  [pml4_table], eax

    ; PDP[0..3] → PD[0..3]
    mov  ecx, 0
.pdp_loop:
    mov  eax, pd_table
    mov  edx, ecx
    shl  edx, 12            ; each PD is 4096 bytes
    add  eax, edx
    or   eax, 0x03
    mov  [pdp_table + ecx * 8], eax
    inc  ecx
    cmp  ecx, 4
    jl   .pdp_loop

    ; PD entries: 2 MiB pages (PS=1), covering 0 – 4 GiB
    mov  ecx, 0
.pd_loop:
    mov  eax, 0x200000      ; 2 MiB
    mul  ecx
    or   eax, 0x83          ; present + writable + huge (2 MiB)
    mov  [pd_table + ecx * 8], eax
    inc  ecx
    cmp  ecx, 2048           ; 4 × 512 entries
    jl   .pd_loop

    ; Load PML4 into CR3
    mov  eax, pml4_table
    mov  cr3, eax

    ; Enable PAE (CR4.PAE)
    mov  eax, cr4
    or   eax, 1 << 5
    mov  cr4, eax

    ; Set EFER.LME (long mode enable)
    mov  ecx, 0xC0000080
    rdmsr
    or   eax, 1 << 8
    wrmsr

    ; Enable paging + protected mode (CR0.PG | CR0.PE)
    mov  eax, cr0
    or   eax, (1 << 31) | (1 << 0)
    mov  cr0, eax

    ; Load 64-bit GDT and far-jump into long mode
    lgdt [gdt64_ptr]
    jmp  gdt64.code:long_mode_entry

.no_cpuid:
.no_long_mode:
    ; Hang — no recovery possible without 64-bit support
    cli
.halt:
    hlt
    jmp .halt

; ─── 64-bit Entry ─────────────────────────────────────────────────────────────
bits 64
extern kernel_main
long_mode_entry:
    mov  ax, gdt64.data
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    ; Set up initial kernel stack
    mov  rsp, stack_top

    ; Clear direction flag
    cld

    ; edi / esi already hold multiboot2 magic / info pointer from 32-bit code.
    ; In the x86-64 SysV ABI the first two integer arguments are rdi, rsi.
    ; The upper halves are zeroed implicitly (32-bit mov zero-extends).
    call kernel_main

    ; Should never return; halt forever
    cli
.halt64:
    hlt
    jmp .halt64
