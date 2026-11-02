.module disk_abi
.area _HEADER (ABS)
.org 0x80bd
    jp seed
    jp capture
    jp dropfiles
seed:
    exx
    ld bc,#0x1234
    ld de,#0x5678
    ld hl,#0x9abc
    exx
    ex af,af'
    xor a
    ld a,#0x55
    scf
    ex af,af'
    ret
capture:
    push ix
    ld ix,#0
    add ix,sp
    ex af,af'
    push af
    ex af,af'
    exx
    push bc
    push de
    push hl
    exx
    pop hl
    ld e,8(ix)
    ld d,9(ix)
    call store
    pop hl
    ld e,6(ix)
    ld d,7(ix)
    call store
    pop hl
    ld e,4(ix)
    ld d,5(ix)
    call store
    pop hl
    ld e,10(ix)
    ld d,11(ix)
    call store
    ld sp,ix
    pop ix
    pop de
    ld hl,#8
    add hl,sp
    ld sp,hl
    push de
    ret
store:
    ld a,l
    ld (de),a
    inc de
    ld a,h
    ld (de),a
    ret
; RAM-only fault injection: consume owned handles without changing the file
; ledger. The protected service must reject all borrowed IDE descriptors.
dropfiles:
    ld hl,#0
    ld bc,#0x7e02
drop_loop:
    ld e,c
    ld d,#0
    ld a,#3
    call 0xfda0
    or a
    jr nz,drop_next
    inc hl
drop_next:
    inc c
    djnz drop_loop
    ret
