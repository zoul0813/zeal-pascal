.module file_low_copy
.globl _zf_low_copy
.area _TEXT
; HL -> private FileLowCopy {address:u16,buffer:u16,count:u8,writing:u8}.
; writing=1 copies caller low data into module staging;0 reverses it.
; Code, request, staging and the C stack stay in page1. Only page2 is remapped.
; No syscall or callback can observe the window; restore page2 and caller IFF.
; A=0 success or6 invalid argument. Invalid calls perform no copy or mapping change.
_zf_low_copy::
    push ix
    push bc
    push de
    push hl
    push hl
    pop ix
    ; A stack in the swapped page would disappear. Module stacks are page1;
    ; a retained observer may also call from page3.
    ld hl,#0
    add hl,sp
    ld a,h
    and #0xc0
    ld e,a
    cp #0x40
    jr z,stack_ok
    cp #0xc0
    jp nz,invalid
stack_ok:
    ; Highest saved/return byte must be in the same retained page as SP.
    ld hl,#9
    add hl,sp
    ld a,h
    and #0xc0
    cp e
    jp nz,invalid
    ; The two AF saves below the current frame must also remain retained.
    ; Validation CALL frames are gone before mapping, so four bytes suffice.
    ld hl,#-4
    add hl,sp
    ld a,h
    and #0xc0
    cp e
    jp nz,invalid
    push ix
    pop hl
    ld bc,#6
    ld d,#0x40
    call span
    jp c,invalid
    ld c,4(ix)
    ld b,#0
    ld a,c
    or a
    jp z,invalid
    cp #129
    jp nc,invalid
    ld l,0(ix)
    ld h,1(ix)
    ld d,#0x60
    call span
    jp c,invalid
    ld l,2(ix)
    ld h,3(ix)
    ld d,#0x40
    call span
    jp c,invalid
    ld a,5(ix)
    cp #2
    jp nc,invalid
    ld a,(0xfe76)
    cp #32
    jp c,invalid
    cp #64
    jp nc,invalid
    ld a,i
    push af
    di
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,(0xfe76)
    out (0xf2),a
    ld l,0(ix)
    ld h,1(ix)
    res 6,h
    set 7,h                     ;6000..7FFF aliases A000..BFFF in page2
    ld e,2(ix)
    ld d,3(ix)
    ld a,5(ix)
    or a
    jr nz,copy
    ex de,hl
copy:
    ldir
    pop af
    out (0xf2),a
    pop af
    jp po,disabled
    ei
disabled:
    pop hl
    pop de
    pop bc
    pop ix
    xor a
    ret
invalid:
    pop hl
    pop de
    pop bc
    pop ix
    ld a,#6
    ret
; HL span start, BC length, D minimum high byte. Exclusive end may equal8000.
; Does not dereference the span or alter BC. Carry means invalid.
span:
    ld a,h
    cp d
    jr c,bad_span
    cp #0x80
    jr nc,bad_span
    add hl,bc
    ld a,h
    cp #0x80
    jr c,good_span
    jr nz,bad_span
    ld a,l
    or a
    jr nz,bad_span
good_span:
    or a
    ret
bad_span:
    scf
    ret
