.module module_unpack
.globl module_unpack_start,module_unpack_end
.area _HEADER (ABS)
.org 0x8000
; HL=build-verified packed constant, DE=owned destination (at most16KiB).
; Private size:u16, LSB-first flag groups, literal/backref codec. Untrusted
; files never enter here. Packaging validates every token and exact expansion.
; All branches are relative, so the compiler can append one shared copy at any
; code origin. Preserve lexical IX and With IY; own all scratch on the stack.
module_unpack_start::
    push ix
    push iy
    push af
    push af
    ld iy,#0
    add iy,sp
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    push bc
    pop ix
flags:
    ld a,(hl)
    inc hl
    ld 0(iy),a
    ld 1(iy),#8
token:
    srl 0(iy)
    jr c,match
    ld a,(hl)
    inc hl
    ld (de),a
    inc de
    dec ix
    jr next
match:
    ld a,(hl)
    inc hl
    ld 2(iy),a
    ld a,(hl)
    inc hl
    push hl
    ld h,a
    ld l,2(iy)
    ld a,l
    and #15
    add a,#3
    ld c,a
    ld b,#0
    push hl
    push ix
    pop hl
    or a
    sbc hl,bc
    push hl
    pop ix
    pop hl
    srl h
    rr l
    srl h
    rr l
    srl h
    rr l
    srl h
    rr l
    inc hl
    push de
    ex de,hl
    or a
    sbc hl,de
    pop de
    ldir                      ; overlapping matches are intentional
    pop hl
next:
    push ix
    pop bc
    ld a,b
    or c
    jr z,done
    dec 1(iy)
    jr nz,token
    jr flags
done:
    pop bc
    pop bc
    pop iy
    pop ix
    ret
module_unpack_end::
