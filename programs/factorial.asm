
; Recursive factorial: computes fact(n) and prints it via TX_INT (0xFF12)
; Calling convention: r0 holds argument and also receives return value.

.org 0x0000
start:
    LDI r0, 5          ; n = 5
    CALL fact
    ST  r0, [0xFF12]   ; print result as integer
    HALT

; r0 = n, returns in r0
fact:
    LDI r1, 0
    CMP r0, r1
    JZ  base
    LDI r1, 1
    CMP r0, r1
    JZ  base

    PUSH r0
    LDI r1, 1
    SUB r0, r1
    CALL fact
    POP r1
    MUL r0, r1
    RET

base:
    LDI r0, 1
    RET
