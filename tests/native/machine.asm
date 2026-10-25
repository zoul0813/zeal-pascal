.module test_machine
.globl _test_machine
.globl _test_complete, _test_stop
.area _TEXT
; The initialized supervisor data-page stop is disjoint from Pascal code.
; This is a test marker, not a product entry point.
_test_complete::
    jp _test_stop
; Test-only observation of IFF, exact caller SP and all four MMU mappings.
; No page, interrupt state, stack or index register is changed.
_test_machine::
    ld a,i
    ld a,#0
    jp po,disabled
    inc a
disabled:
    ld (hl),a
    inc hl
    ex de,hl
    ld hl,#0
    add hl,sp
    ld a,l
    ld (de),a
    inc de
    ld a,h
    ld (de),a
    inc de
    xor a
    in a,(0xf0)
    ld (de),a
    inc de
    ld a,#0x40
    in a,(0xf1)
    ld (de),a
    inc de
    ld a,#0x80
    in a,(0xf2)
    ld (de),a
    inc de
    ld a,#0xc0
    in a,(0xf3)
    ld (de),a
    ret
