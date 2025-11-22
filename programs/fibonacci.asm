; Fibonacci sequence: print fib(1)..fib(10)
; r0 = argument/result for functions
; Uses recursion; caller-saved regs, so we use the stack to preserve values.

.org 0x0000
start:
    LDI r3, 1          ; i = 1
    LDI r5, 10         ; remaining = 10

loop:
    ; call fib(i)
    MOV r0, r3
    CALL fib
    ST  r0, [0xFF12]   ; print value (decimal + newline)

    ; i++, remaining--
    ADDI r3, 1
    SUBI r5, 1
    LDI  r1, 0
    CMP  r5, r1
    JNZ  loop

    HALT

; ----------------------------------------
; fib(n): classic recursive Fibonacci
; r0 = n, returns fib(n) in r0
fib:
    ; if (n == 0) return 0
    LDI r1, 0
    CMP r0, r1
    JZ  fib_base0
    ; if (n == 1) return 1
    LDI r1, 1
    CMP r0, r1
    JZ  fib_base1

    ; Save n
    PUSH r0

    ; --- compute fib(n-1) ---
    LDI r1, 1
    SUB r0, r1          ; r0 = n-1
    CALL fib            ; r0 = fib(n-1)

    ; Restore n, stash fib(n-1) on stack
    POP r1              ; r1 = n
    PUSH r0             ; [SP-1] = fib(n-1)

    ; --- compute fib(n-2) ---
    LDI r0, 2
    SUB r1, r0          ; r1 = n-2
    MOV r0, r1          ; r0 = n-2 (arg)
    CALL fib            ; r0 = fib(n-2)

    ; add saved fib(n-1)
    POP r1              ; r1 = fib(n-1)
    ADD r0, r1
    RET

fib_base0:
    LDI r0, 0
    RET
fib_base1:
    LDI r0, 1
    RET
