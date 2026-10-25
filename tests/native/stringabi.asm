; Independent native ABI probe, embedded as typed-constant code by STRINGABI.PAS.
; No index/alternate registers are changed. Deliberately return HL=DEAD: string
; results belong to the caller's fixed stack slot, not an HL register promise.
.module stringabi
.area _HEADER (ABS)
.org 0x4000
    pop de                      ; return word
    ld hl,#0
    add hl,sp                   ; final Integer actual
    ld a,(hl)
    push af
    inc hl
    inc hl
    ld c,(hl)                   ; full current string length, not formal capacity
    ld b,#0
    inc bc
    add hl,bc                   ; first Integer actual
    ld b,(hl)
    inc hl
    inc hl                      ; caller-owned four-byte string result slot
    ld (hl),#3
    inc hl
    ld (hl),b
    inc hl
    dec c
    ld (hl),c
    inc hl
    pop af
    ld (hl),a
    dec hl
    dec hl
    dec hl
    ld sp,hl
    ld hl,#0xdead
    push de
    ret
