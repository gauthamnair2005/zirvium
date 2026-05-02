; arch/x64/isr_stubs.asm
; Zirvium Kernel — 256 ISR stub entries + dispatch
;
; Each stub pushes a dummy error-code (for exceptions that don't provide one),
; the interrupt number, saves all GP registers, and calls the common C handler
; `isr_dispatch(cpu_state_t *)`.

bits 64

extern isr_dispatch          ; void isr_dispatch(cpu_state_t *state)

; ── Common ISR path ───────────────────────────────────────────────────────────
isr_common_stub:
    ; At this point the stack holds (top→bottom):
    ;   rax, rcx, rdx, rbx, rbp, rsi, rdi, r8–r15  ← pushed below
    ;   int_no, err_code                              ← pushed by stub
    ;   rip, cs, rflags, rsp, ss                     ← pushed by CPU

    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov  rdi, rsp            ; cpu_state_t * as first argument
    call isr_dispatch

    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  r11
    pop  r10
    pop  r9
    pop  r8
    pop  rdi
    pop  rsi
    pop  rbp
    pop  rbx
    pop  rdx
    pop  rcx
    pop  rax

    add  rsp, 16             ; discard int_no + err_code
    iretq

; ── Macro helpers ─────────────────────────────────────────────────────────────
%macro ISR_NOERR 1
isr_stub_%1:
    push qword 0             ; dummy error code
    push qword %1            ; interrupt number
    jmp  isr_common_stub
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    ; CPU already pushed an error code
    push qword %1
    jmp  isr_common_stub
%endmacro

; ── Exception stubs (0 – 31) ─────────────────────────────────────────────────
ISR_NOERR  0   ; #DE Divide Error
ISR_NOERR  1   ; #DB Debug
ISR_NOERR  2   ; NMI
ISR_NOERR  3   ; #BP Breakpoint
ISR_NOERR  4   ; #OF Overflow
ISR_NOERR  5   ; #BR Bound Range Exceeded
ISR_NOERR  6   ; #UD Invalid Opcode
ISR_NOERR  7   ; #NM Device Not Available
ISR_ERR    8   ; #DF Double Fault
ISR_NOERR  9   ; Coprocessor Segment Overrun (reserved)
ISR_ERR   10   ; #TS Invalid TSS
ISR_ERR   11   ; #NP Segment Not Present
ISR_ERR   12   ; #SS Stack-Segment Fault
ISR_ERR   13   ; #GP General Protection Fault
ISR_ERR   14   ; #PF Page Fault
ISR_NOERR 15   ; Reserved
ISR_NOERR 16   ; #MF x87 Floating-Point Exception
ISR_ERR   17   ; #AC Alignment Check
ISR_NOERR 18   ; #MC Machine Check
ISR_NOERR 19   ; #XF SIMD Floating-Point Exception
ISR_NOERR 20   ; #VE Virtualisation Exception
ISR_ERR   21   ; #CP Control Protection Exception
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28   ; #HV Hypervisor Injection
ISR_ERR   29   ; #VC VMM Communication Exception
ISR_ERR   30   ; #SX Security Exception
ISR_NOERR 31

; ── IRQ stubs (32 – 47 = PIC IRQ 0–15) ──────────────────────────────────────
%assign i 32
%rep 224
ISR_NOERR i
%assign i i+1
%endrep

; ── Stub pointer table (exported so idt.c can iterate) ───────────────────────
section .data
global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    dq isr_stub_%+i
%assign i i+1
%endrep
