; Independent Pascal ABI wrapper: (operation,argument:Integer; var error,value).
; A high operation bit requests DI for the gate; otherwise EI. The wrapper
; checks BC/HL, IX/IY, alternate registers and IFF, then restores its caller.
.module workerabi
.area _HEADER (ABS)
.org 0x8003
    push ix
    ld ix,#0
    add ix,sp
    push iy
    exx
    push bc
    push de
    push hl
    exx
    ex af,af'
    push af
    ex af,af'
    ld a,i
    ld hl,#0
    jp po,old_iff
    inc l
old_iff:
    push hl
    push ix
    ld iy,#0x5a6b
    exx
    ld bc,#0x1122
    ld de,#0x3344
    ld hl,#0x5566
    exx
    ex af,af'
    ld hl,#0x9944
    push hl
    pop af
    ex af,af'
    ld bc,#0x1234
    ld hl,#0x5678
    bit 7,11(ix)
    jr nz,force_off
    ei
    jr arguments
force_off:
    di
arguments:
    ld a,10(ix)
    ld e,8(ix)
    ld d,9(ix)
    call 0xfda0
    push af
    push de
    ld a,b
    cp #0x12
    jp nz,bad
    ld a,c
    cp #0x34
    jp nz,bad
    ld a,h
    cp #0x56
    jp nz,bad
    ld a,l
    cp #0x78
    jp nz,bad
    push iy
    pop hl
    ld a,h
    cp #0x5a
    jp nz,bad
    ld a,l
    cp #0x6b
    jp nz,bad
    push ix
    pop de
    ld hl,#4
    add hl,sp
    ld c,(hl)
    inc hl
    ld b,(hl)
    ld h,d
    ld l,e
    or a
    sbc hl,bc
    jp nz,bad
    exx
    ld a,b
    cp #0x11
    jr nz,bad_alt
    ld a,c
    cp #0x22
    jr nz,bad_alt
    ld a,d
    cp #0x33
    jr nz,bad_alt
    ld a,e
    cp #0x44
    jr nz,bad_alt
    ld a,h
    cp #0x55
    jr nz,bad_alt
    ld a,l
    cp #0x66
    jr nz,bad_alt
    exx
    ex af,af'
    push af
    ex af,af'
    pop hl
    ld a,h
    cp #0x99
    jr nz,bad
    ld a,l
    cp #0x44
    jr nz,bad
    ld a,i
    jp po,gate_off
    bit 7,11(ix)
    jr nz,bad
    jr good
gate_off:
    bit 7,11(ix)
    jr z,bad
good:
    ld c,#0
    jr results
bad_alt:
    exx
bad:
    ld c,#1
results:
    pop de
    pop af
    pop ix
    bit 0,c
    jr z,status
    ld a,#255
status:
    ld l,6(ix)
    ld h,7(ix)
    ld (hl),a
    inc hl
    ld (hl),#0
    ld l,4(ix)
    ld h,5(ix)
    ld (hl),e
    inc hl
    ld (hl),d
    pop hl
    ld c,l
    ex af,af'
    pop af
    ex af,af'
    exx
    pop hl
    pop de
    pop bc
    exx
    pop iy
    pop ix
    pop hl
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    inc sp
    push hl
    ld a,c
    or a
    jr z,return_off
    ei
    ret
return_off:
    di
    ret
