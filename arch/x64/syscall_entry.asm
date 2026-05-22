; arch/x64/syscall_entry.asm
; Zirvium Kernel — SYSCALL / SYSRETQ entry stub (x86-64)
;
; Invoked by the CPU when userspace executes the SYSCALL instruction.
;
; CPU state on entry
;   RAX    = syscall number
;   RDI    = arg1, RSI = arg2, RDX = arg3
;   R10    = arg4  (RCX is clobbered by SYSCALL; zirvlibc puts arg4 in R10)
;   R8     = arg5, R9 = arg6
;   RCX    = saved user RIP
;   R11    = saved user RFLAGS
;   RSP    = user RSP (CPU does NOT switch the stack)
;   IF     = 0  (FMASK clears it)
;
; This stub:
;   1. Switches to the shared kernel syscall stack
;   2. Saves the return context (user RSP / RFLAGS / RIP)
;   3. Saves callee-saved registers
;   4. Reshuffles register arguments for the C calling convention and calls
;      syscall_dispatch(num, a1, a2, a3, a4, a5, a6)
;   5. Restores all saved state
;   6. Returns to user mode via o64 sysret (SYSRETQ)
;
; Stack alignment note
;   kernel_stack_top is 16-byte aligned.  After the 9 saves below the stack
;   is at offset -72 (RSP % 16 == 8).  One additional push (a6) brings it to
;   -80 (RSP % 16 == 0).  CALL then pushes 8 bytes, leaving the callee with
;   RSP % 16 == 8, satisfying the System V AMD64 ABI requirement.

[BITS 64]
default rel                     ; all bare symbol references are RIP-relative

section .text

extern syscall_dispatch
extern syscall_kernel_stack_top

global syscall_entry

section .bss
align 8
; Per-CPU save slot for the user RSP on SYSCALL entry.
; TODO(SMP): replace with a per-CPU structure (e.g. via GSBASE MSR) so that
; concurrent syscalls on different cores do not overwrite each other's slot.
tmp_user_rsp:   resq 1          ; temporary user-RSP save (single-CPU)

section .text

syscall_entry:
    ; ── 1. Switch to the kernel syscall stack ─────────────────────────────
    mov     [tmp_user_rsp], rsp
    mov     rsp, [syscall_kernel_stack_top]

    ; ── 2. Save return context on kernel stack ────────────────────────────
    push    qword [tmp_user_rsp]     ; user RSP
    push    r11                      ; user RFLAGS
    push    rcx                      ; user RIP

    ; ── 3. Save callee-saved registers ────────────────────────────────────
    push    rbp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15

    ; ── 4. Save caller-saved argument registers ──────────────────────────
    ; The C dispatcher may clobber rdi, rsi, rdx, r8, r9, r10.
    ; They MUST be restored before returning so user-space sees them
    ; preserved (Linux ABI guarantees only rcx / r11 are clobbered).
    push    r10
    push    r9
    push    r8
    push    rdx
    push    rsi
    push    rdi

    ; ── 5. Build C arguments ──────────────────────────────────────────────
    ; Target: syscall_dispatch(num, a1, a2, a3, a4, a5, a6)
    ; C ABI (System V):  rdi  rsi  rdx  rcx   r8   r9  [rsp+8]
    ; Available on entry: rax  rdi  rsi  rdx  r10   r8      r9
    ;
    ; Push a6 (original r9 — still live since we saved it above) as the
    ; 7th C argument before modifying r9.
    push    r9                       ; a6 → [rsp+8] after CALL (7th C arg)

    mov     r9, r8                   ; a5 → r9  (6th C arg)
    mov     r8, r10                  ; a4 → r8  (5th C arg)
    mov     rcx, rdx                 ; a3 → rcx (4th C arg)
    mov     rdx, rsi                 ; a2 → rdx (3rd C arg)
    mov     rsi, rdi                 ; a1 → rsi (2nd C arg)
    mov     rdi, rax                 ; num → rdi (1st C arg)

    ; ── 6. Dispatch ───────────────────────────────────────────────────────
    call    syscall_dispatch
    add     rsp, 8                   ; discard pushed a6

    ; Return value (or negated errno) is already in RAX.

    ; ── 7. Restore caller-saved argument registers ────────────────────────
    pop     rdi
    pop     rsi
    pop     rdx
    pop     r8
    pop     r9
    pop     r10

    ; ── 8. Restore callee-saved registers ─────────────────────────────────
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp

    ; ── 9. Restore context and return to user mode ─────────────────────────
    pop     rcx                      ; user RIP  → RCX (used by SYSRETQ)
    pop     r11                      ; user RFLAGS → R11 (used by SYSRETQ)
    pop     rsp                      ; user RSP

    o64 sysret

; Mark the stack as non-executable (suppresses GNU-stack linker warning)
section .note.GNU-stack noalloc noexec nowrite progbits
