; Register-based string/arithmetic helpers. No SDCC library is linked.
; Existing private call sites use HL/DE for the first two arguments and
; callee-popped stack words for further arguments. IX is always preserved.
fragment f_strlen
_strlen:
    xor a
    ld b,a
    ld c,a
    cpir
    ld hl,#-1
    sbc hl,bc
    ex de,hl
    ret
endfragment f_strlen

fragment f_strcat
_strcat:
    ld c,l
    ld b,h
strcat_find:
    ld a,(hl)
    or a
    jr z,strcat_copy
    inc hl
    jr strcat_find
strcat_copy:
    ld a,(de)
    inc de
    ld (hl),a
    inc hl
    or a
    jr nz,strcat_copy
    ld e,c
    ld d,b
    ret
endfragment f_strcat

fragment f_memchr
; HL=buffer, E=needle, stack=count. Returns DE=match or zero.
_memchr:
    ld a,e
    pop iy
    pop bc
    push iy
    ld e,a
memchr_next:
    ld a,b
    or c
    jr z,memchr_missing
    ld a,e
    cp (hl)
    jr z,memchr_found
    inc hl
    dec bc
    jr memchr_next
memchr_found:
    ex de,hl
    ret
memchr_missing:
    ld de,#0
    ret
endfragment f_memchr

fragment f_sdcc_enter_ix
; Compatibility with the retained stack-frame convention inside file logic.
___sdcc_enter_ix:
    pop hl
    push ix
    ld ix,#0
    add ix,sp
    jp (hl)
endfragment f_sdcc_enter_ix

fragment f_muluint2ulong
; Unsigned 16x16 -> 32, result HL:DE. File positions are unsigned.
___muluint2ulong:
    ld iy,#0
    ld b,#16
multiply_next:
    add iy,iy
    adc hl,hl
    jr nc,multiply_skip
    add iy,de
    jr nc,multiply_skip
    inc hl
multiply_skip:
    djnz multiply_next
    push iy
    pop de
    ret
endfragment f_muluint2ulong

fragment f_strcmp
_strcmp:
strcmp_next:
    ld a,(de)
    ld b,a
    inc de
    ld a,(hl)
    inc hl
    sub b
    ld c,a
    or a
    jr nz,strcmp_result
    or b
    jr nz,strcmp_next
strcmp_result:
    ld a,c
    rlca
    sbc a,a
    ld d,a
    ld e,c
    ret
endfragment f_strcmp

fragment f_memcmp
; HL=left, DE=right, stack=count. Exact unsigned-byte difference in DE.
_memcmp:
    pop iy
    pop bc
    push iy
memcmp_next:
    ld a,b
    or c
    jr z,memcmp_equal
    ld a,(de)
    cp (hl)
    jr nz,memcmp_different
    inc de
    inc hl
    dec bc
    jr memcmp_next
memcmp_different:
    ld c,a
    ld a,(hl)
    sub c
    ld e,a
    ld d,#0
    ret nc
    dec d
    ret
memcmp_equal:
    ld de,#0
    ret
endfragment f_memcmp

fragment f_sdcc_call_hl
___sdcc_call_hl:
    jp (hl)
endfragment f_sdcc_call_hl
