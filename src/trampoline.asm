.module trampoline
.area _HEADER (ABS)
.org 0xfd00
    ld a,(0xfc0c)
    ld de,#0x4000
    call map_page
    or a
    jr nz,failed
    ld a,(0xfc0d)
    ld de,#0x8000
    call map_page
    or a
    jr nz,failed
    ld a,(0xfc0f)
    or a
    jr z,enter
    ei
enter:
    jp 0x40a3
failed:
    ld (0xfc1c),a
    ld a,#6
    ld (0xfc1a),a
    ld a,#4
    ld (0xfc1b),a
    jp 0xfd80
map_page:
    ld b,#0
    ld c,b
    or a
    rra
    rr b
    rra
    rr b
    ld h,a
    ld l,#23
    rst 8
    ret
.org 0xfd80
    di
    ld a,(0xfc09)
    ld de,#0x4000
    call map_page
    ld a,(0xfc0a)
    ld de,#0x8000
    call map_page
    ld hl,(0xfc12)
    jp (hl)
