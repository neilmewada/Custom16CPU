
.org 0x0000
start:
    LDI r0, msg
    ST  r0, [0xFF10]   ; print string
    HALT

msg:
    .asciiz "Hello, World!"
