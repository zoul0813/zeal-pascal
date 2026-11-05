.module fragment_probe
.globl _fragment_run,_fragment_before,_fragment_after,_fragment_value,_test_machine
.area _HOME
; A=physical output page, DE=native entry. The observer uses identical saved
; frames on each side of the call; C register spills cannot skew its SP oracle.
; Continuation and mapping core stay in page1; caller/test code may use page2.
_fragment_run::
    push ix
    push af
    push de
    ld hl,#_fragment_before
    call _test_machine
    pop de
    pop af
    ld c,a
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,c
    out (0xf2),a
    ld hl,#resume
    push hl
    ex de,hl
    jp (hl)
resume:
    pop af
    out (0xf2),a
    ld (_fragment_value),hl
    push af
    push de
    ld hl,#_fragment_after
    call _test_machine
    pop de
    pop af
    pop ix
    ld de,(_fragment_value)      ; SDCC call1 returns a16-bit result in DE
    ret
