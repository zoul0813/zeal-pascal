; Independent public ABI: pointer word, Integer word -> pointer in HL.
; Callee consumes both words and leaves alternate/index registers untouched.
.module pointerabi
.area _HEADER (ABS)
.org 0x4000
    pop de
    pop bc
    pop hl
    add hl,bc
    push de
    ret
