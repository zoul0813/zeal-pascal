; Independent public ABI probe: Byte word, full 32-byte set, Integer word.
; XOR the scalar bytes plus both ends of the set, remove all 36 argument bytes.
; IX/IY and alternate registers remain untouched.
.module setabi
.area _HEADER (ABS)
.org 0x4000
    pop de
    ld hl,#0
    add hl,sp
    ld a,(hl)
    inc hl
    inc hl
    xor (hl)
    ld bc,#31
    add hl,bc
    xor (hl)
    inc hl
    xor (hl)
    inc hl
    xor (hl)
    inc hl
    ld sp,hl
    ld l,a
    ld h,#0
    push de
    ret
