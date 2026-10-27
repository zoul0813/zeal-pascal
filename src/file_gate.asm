.module file_gate
.globl file_gate_end
.area _HEADER (ABS)
.org 0xfe90
; HL=caller-owned job, A=0 operation,1 cleanup,2 terminal cleanup/free.
; Only this retained gate changes prefix mappings. C may use both index and
; alternate sets; restore them before returning to Pascal/With code. The
; complete stack remains below FC00. No interrupt-state change is needed.
    ld (0xfe7e),a
    xor a
    ld (0xfe7f),a              ; secondary module-page release error
    push hl
    push ix
    push iy
    ex af,af'
    push af
    ex af,af'
    exx
    push bc
    push de
    push hl
    exx
    ld a,(0xfe74)
    out (0xf1),a
    ld hl,#12
    add hl,sp
    ld e,(hl)
    inc hl
    ld d,(hl)
    ex de,hl
    ld a,(0xfe7e)
    or a
    jr z,dispatch
    call 0x4003
    jr returned
dispatch:
    call 0x4000
returned:
    ld a,(0xfe76)
    out (0xf1),a
    ld a,(0xfe7e)
    cp #2
    jr nz,restore
    ld a,(0xfe74)
    ld e,a
    ld d,#0
    xor a
    ld (0xfe74),a              ; revoke entry before releasing its backing
    ld a,(0xfe78)
    or a
    jr z,native_free
    ld a,#1
    call 0xfda0
    jr freed
native_free:
    ld b,e
    ld l,#26
    rst 8
freed:
    ld (0xfe7f),a
restore:
    exx
    pop hl
    pop de
    pop bc
    exx
    ex af,af'
    pop af
    ex af,af'
    pop iy
    pop ix
    pop hl
    ret
file_gate_end::
