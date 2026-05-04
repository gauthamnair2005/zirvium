; arch/x64/boot.asm
; Zirvium Kernel - x86-64 Multiboot2 Boot Entry
; Follows the Multiboot2 specification (GNU GRUB compatible)
;
; Layout in the final ELF:
;   .multiboot2  — Multiboot2 header tags (VMA = LMA = PHYS_BOOT)
;   .boot_text   — 32-bit bootstrap + 64-bit bridge (VMA = LMA = PHYS_BOOT+)
;   .boot_data   — 64-bit GDT descriptor table (VMA = LMA)
;   .boot_bss    — page tables + initial stack (VMA = LMA, NOBITS)
;
; _start runs at its physical address in 32-bit protected mode (as delivered
; by GRUB/Multiboot2).  It sets up page tables that provide:
;   - identity map of 0 – 4 GiB via PML4[0] (so the boot stub keeps running)
;   - higher-half map of 0xFFFFFFFF80000000+ via PML4[511] (kernel VMA)
; Then it enables long mode and calls kernel_main via an absolute 64-bit jump.

bits 32

; ─── Multiboot2 Header ────────────────────────────────────────────────────────
MULTIBOOT2_MAGIC    equ 0xE85250D6
MULTIBOOT2_ARCH     equ 0           ; i386 protected mode
MULTIBOOT2_LENGTH   equ (mb2_header_end - mb2_header_start)
MULTIBOOT2_CHECKSUM equ (-(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + MULTIBOOT2_LENGTH) & 0xFFFFFFFF)

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

; ─── Boot BSS: page tables + initial stack ────────────────────────────────────
; All of these reside at their physical address (VMA = LMA < 2 MiB) so
; 32-bit code can reference them directly without a V2P conversion.
section .boot_bss nobits alloc noexec write
align 4096
pml4_table:   resb 4096
pdp_low:      resb 4096          ; PDP for identity map (PML4[0])
pdp_high:     resb 4096          ; PDP for higher-half (PML4[511])
pd_low:       resb 4096 * 4      ; 4 PDs for 4 × 512 × 2 MiB = 4 GiB identity
pd_high:      resb 4096          ; 1 PD for higher-half kernel (≥ 8 MiB mapped)
align 16
stack_bottom: resb 32768         ; 32 KiB initial kernel stack
stack_top:

; ─── Boot Data: 64-bit GDT ────────────────────────────────────────────────────
section .boot_data alloc noexec write
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

; ─── Boot Text: 32-bit bootstrap code ────────────────────────────────────────
section .boot_text alloc exec nowrite

global _start
_start:
    ; Save Multiboot2 info pointer (ebx) and magic (eax) in caller-save regs.
    ; They will be passed as the first two arguments to kernel_main.
    mov edi, eax            ; magic  → edi (rdi in 64-bit SysV ABI)
    mov esi, ebx            ; info*  → esi (rsi in 64-bit SysV ABI)

    ; ── Check CPUID support ──────────────────────────────────────────────────
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

    ; ── Check long-mode support (CPUID 0x80000001) ──────────────────────────
    mov  eax, 0x80000000
    cpuid
    cmp  eax, 0x80000001
    jb   .no_long_mode
    mov  eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz   .no_long_mode

    ; ── Build page tables ────────────────────────────────────────────────────
    ; All symbols below have VMA = LMA = physical address (boot sections).

    ; PML4[0] → pdp_low  (identity mapping 0 – 4 GiB)
    mov  eax, pdp_low
    or   eax, 3
    mov  [pml4_table], eax

    ; PML4[511] → pdp_high  (higher-half kernel mapping)
    mov  eax, pdp_high
    or   eax, 3
    mov  [pml4_table + 511 * 8], eax

    ; pdp_low[0..3] → pd_low[0..3]  (each PD covers 1 GiB of identity range)
    mov  ecx, 0
.pdp_low_loop:
    mov  eax, pd_low
    mov  edx, ecx
    shl  edx, 12            ; each PD is 4096 bytes
    add  eax, edx
    or   eax, 3
    mov  [pdp_low + ecx * 8], eax
    inc  ecx
    cmp  ecx, 4
    jl   .pdp_low_loop

    ; pd_low entries: 2 MiB pages covering physical 0 – 4 GiB (identity)
    mov  ecx, 0
.pd_low_loop:
    mov  eax, 0x200000      ; 2 MiB per entry
    mul  ecx                ; edx:eax = eax * ecx  (edx = 0 for ecx < 2048)
    or   eax, 0x83          ; present + writable + huge (2 MiB page)
    mov  [pd_low + ecx * 8], eax
    inc  ecx
    cmp  ecx, 2048          ; 4 PDPs × 512 entries = 2048 total
    jl   .pd_low_loop

    ; pdp_high[510] → pd_high
    ; VA 0xFFFFFFFF80000000 falls in PML4[511] → PDP[510] (computed from
    ; the address bit-fields: PML4=511, PDP=510, PD=0).
    mov  eax, pd_high
    or   eax, 3
    mov  [pdp_high + 510 * 8], eax

    ; pd_high entries: 2 MiB pages mapping VA 0xFFFFFFFF80000000 → PA 0x200000
    ; (PHYS_KERNEL = 0x200000, which is 2 MiB aligned — required for huge pages)
    ; Maps 4 × 2 MiB = 8 MiB of kernel virtual address space.
    mov  ecx, 0
.pd_high_loop:
    mov  eax, ecx
    shl  eax, 21            ; eax = ecx * 2 MiB
    add  eax, 0x200000      ; PA = PHYS_KERNEL + ecx * 2 MiB
    or   eax, 0x83          ; present + writable + huge
    mov  [pd_high + ecx * 8], eax
    inc  ecx
    cmp  ecx, 4
    jl   .pd_high_loop

    ; ── Load PML4 into CR3 ───────────────────────────────────────────────────
    mov  eax, pml4_table
    mov  cr3, eax

    ; ── Enable PAE (CR4.PAE) ─────────────────────────────────────────────────
    mov  eax, cr4
    or   eax, 1 << 5
    mov  cr4, eax

    ; ── Set EFER.LME + EFER.NXE (long mode + no-execute enable) ────────────────
    mov  ecx, 0xC0000080
    rdmsr
    or   eax, (1 << 8) | (1 << 11)   ; LME = bit 8, NXE = bit 11
    wrmsr

    ; ── Enable paging + protected mode (CR0.PG | CR0.PE) ────────────────────
    mov  eax, cr0
    or   eax, (1 << 31) | (1 << 0)
    mov  cr0, eax

    ; ── Load 64-bit GDT and far-jump into long mode ──────────────────────────
    ; gdt64_ptr is in .boot_data (VMA = LMA = physical) so [gdt64_ptr] is
    ; a valid physical address reachable via the identity map.
    lgdt [gdt64_ptr]
    jmp  gdt64.code:long_mode_entry

.no_cpuid:
.no_long_mode:
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

    ; Set up initial kernel stack (boot_bss, reachable via identity map)
    mov  rsp, stack_top

    ; Clear direction flag
    cld

    ; edi / esi already hold multiboot2 magic / info pointer from 32-bit code.
    ; In the x86-64 SysV ABI the first two integer arguments are rdi, rsi.
    ; 32-bit mov zero-extends to 64 bits, so the upper halves are already 0.

    ; kernel_main resides at its high virtual address (0xFFFFFFFF80000000+).
    ; The higher-half page table mapping set up above makes it reachable.
    ; We must use an absolute 64-bit call (not a 32-bit relative call) because
    ; the distance from this stub to the kernel text exceeds ±2 GiB.
    mov  rax, kernel_main
    call rax

    ; Should never return; halt forever
    cli
.halt64:
    hlt
    jmp .halt64

; Mark stack as non-executable
section .note.GNU-stack noalloc noexec nowrite progbits
