
.org 0x0000
start:
    LDI r0, 0          ; last = 0
loop:
    LD  r1, [0xFF20]   ; r1 = timer
    CMP r1, r0
    JZ  loop           ; spin until timer changes
    MOV r0, r1         ; last = current
    LDI r2, 'T'
    ST  r2, [0xFF00]   ; print char
    JMP loop
