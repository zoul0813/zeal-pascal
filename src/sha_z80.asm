.module sha_rotations
.globl _sha_rr2,_sha_rr6,_sha_rr7,_sha_rr11,_sha_rr13
.globl _sha_rr17,_sha_rr18,_sha_rr19,_sha_rr22,_sha_rr25
.area _TEXT
; SDCC 32-bit argument/result: DE low word, HL high word. Reduce each
; rotation to byte permutations and at most three register-only bit steps.
; No memory, index registers or alternate registers are changed.
.macro byte_right
    ld a,e
    ld e,d
    ld d,l
    ld l,h
    ld h,a
.endm
_sha_rr2::
    ld b,#2
    jr right
_sha_rr6::
    byte_right
    ld b,#2
    jr left
_sha_rr7::
    byte_right
    ld b,#1
    jr left
_sha_rr11::
    byte_right
    ld b,#3
    jr right
_sha_rr13::
    ex de,hl
    ld b,#3
    jr left
_sha_rr17::
    ex de,hl
    ld b,#1
    jr right
_sha_rr18::
    ex de,hl
    ld b,#2
    jr right
_sha_rr19::
    ex de,hl
    ld b,#3
    jr right
_sha_rr22::
    ex de,hl
    byte_right
    ld b,#2
    jr left
_sha_rr25::
    ex de,hl
    byte_right
    ld b,#1
right:
    ld a,e
    rrca
    rr h
    rr l
    rr d
    rr e
    djnz right
    ret
left:
    ld a,h
    rlca
    rl e
    rl d
    rl l
    rl h
    djnz left
    ret
