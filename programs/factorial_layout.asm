.org 0x0000
start:
    ; compute and print fact(6)
    LDI  r0, 6
    CALL fact
    ST   r0, [0xFF12]       ; print 720
    HALT

fact:
    ; ---- prologue: save callee-saved regs we use ----
    PUSH r1
    PUSH r2

    ; if (n == 0) return 1
    LDI  r1, 0
    CMP  r0, r1
    JZ   fact_base

    ; if (n == 1) return 1
    LDI  r1, 1
    CMP  r0, r1
    JZ   fact_base

    ; ---- recursive case ----
    PUSH r0            ; save n

    ; r0 = n - 1
    LDI  r1, 1
    SUB  r0, r1
    CALL fact          ; r0 = fact(n-1)

    POP  r1            ; r1 = n
    MUL  r0, r1        ; r0 = n * fact(n-1)
    JMP  fact_ret

fact_base:
    LDI  r0, 1

fact_ret:
    ; ---- epilogue: restore callee-saved regs and return ----
    POP  r2
    POP  r1
    RET
