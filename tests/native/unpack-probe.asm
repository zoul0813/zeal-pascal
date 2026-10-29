.module unpack_probe
.globl _unpack_probe,_test_unpack_decoder
.globl _unpack_probe_restore,_unpack_probe_after_restore
.area _UNPACK_TEST (ABS)
.org 0x8000
; A=owned destination page, DE=packed source outside the4000 window.
; Execute from8000 while the prefix is mapped, exactly as generated callers do.
; Preserve C's original IX/IY, and separately check the decoder's preservation.
_unpack_probe::
    push ix
    push iy
    ex de,hl
    ld c,a
    ld a,#0x40
    in a,(0xf1)
    push af
    ld a,c
    out (0xf1),a
    ld ix,#0x1234
    ld iy,#0x5678
    ld de,#0x4000
    call _test_unpack_decoder
    pop af
_unpack_probe_restore::
    out (0xf1),a
_unpack_probe_after_restore::
    ld e,#1
    push ix
    pop hl
    ld bc,#0x1234
    or a
    sbc hl,bc
    jr nz,bad
    push iy
    pop hl
    ld bc,#0x5678
    or a
    sbc hl,bc
    jr z,done
bad:
    ld e,#0
done:
    ld a,e
    pop iy
    pop ix
    ret
