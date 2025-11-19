; Recursive Fibonacci for Crystal-16 (caller-saved regs)
; r0 = n, returns fib(n) in r0. Uses the stack to preserve fib(n-1).

.org 0x0000
start:
    LDI r0, 8          ; choose n (fib(8)=21)
    CALL fib
    ST  r0, [0xFF12]
    HALT

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
    SUB r0, r1
    CALL fib                 ; r0 = fib(n-1)

    ; Re-stack so fib(n-1) is kept while we restore n
    POP r1                   ; r1 = n
    PUSH r0                  ; push fib(n-1)
    PUSH r1                  ; push n
    POP r0                   ; r0 = n   (top of stack now: fib(n-1))

    ; --- compute fib(n-2) ---
    LDI r1, 2
    SUB r0, r1
    CALL fib                 ; r0 = fib(n-2)

    ; add saved fib(n-1)
    POP r1                   ; r1 = fib(n-1)
    ADD r0, r1               ; r0 = fib(n-2) + fib(n-1)
    RET

fib_base0:
    LDI r0, 0
    RET
fib_base1:
    LDI r0, 1
    RET
