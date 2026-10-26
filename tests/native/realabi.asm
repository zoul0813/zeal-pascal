; Independent public Real wire probe: Real, Integer, Real -> Real in BCDEHL.
; Transform all six bytes using both arguments and both Integer bytes.
; Preserve IX/IY and alternate sets, and consume exactly14 argument bytes.
.module realabi
.area _HEADER (ABS)
.org 0x4000
    push ix
    ld ix,#0
    add ix,sp
    ld a,12(ix)
    xor 4(ix)
    ld l,a
    ld a,13(ix)
    xor 5(ix)
    xor 10(ix)
    ld h,a
    ld a,14(ix)
    xor 6(ix)
    ld e,a
    ld a,15(ix)
    xor 7(ix)
    ld d,a
    ld a,16(ix)
    xor 8(ix)
    ld c,a
    ld a,17(ix)
    xor 9(ix)
    xor 11(ix)
    ld b,a
    pop ix
    pop af                      ; return word, after all flag-changing work
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    push af
    ret
