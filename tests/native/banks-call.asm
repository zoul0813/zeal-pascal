.module banks_call
.globl _bank_call, _bank_mapping, _test_nested
.area _HOME
; Resident component fixture: no caller continuation occupies the replaced
; window. This is NOT the generated-Pascal bank-call ABI.
_bank_mapping::
    ld a,#0x80
    in a,(0xf2)
    ret
_bank_call::                  ; SDCC call1: A=physical page, DE=entry
    ld (call_target+1),de
    ld c,a
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,c
    out (0xf2),a
call_target:
    call 0
    pop af
    out (0xf2),a
    ret
.area _BANK_CALLBACK
; Independent fixed fixture ABI; the loader neither patches these calls nor
; reads this code. CMake checks the remaining resident image stays below7000.
    push af
    push bc
    push de
    push hl
    push ix
    push iy
    call _test_nested
    pop iy
    pop ix
    pop hl
    pop de
    pop bc
    pop af
    ret
