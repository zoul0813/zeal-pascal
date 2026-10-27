.module test_machine
.globl _test_machine
.globl _test_complete, _test_stop
.equ _test_stop, 0xfc00
.area _TEXT
; Generated static/code storage can now extend into page3, up to F400.
; FC00 is a nonexecutable completion context in Pascal children. Install this
; supervisor-only stop after all checks, abandoning its finished C stack.
_test_complete::
    ld sp,#0xfc00
    ld a,#0x76
    ld (_test_stop),a
    ld hl,#0xfd18
    ld (_test_stop+1),hl
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
