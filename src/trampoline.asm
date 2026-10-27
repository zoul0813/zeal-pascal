.module trampoline
.globl trampoline_enter_end,worker_control_data,worker_gate,worker_return,worker_end
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
trampoline_enter_end::
.org 0xfd50
worker_control_data::
    .ascii "ZWRK"
    .db 1,0                    ; version; disabled until the parent prepares it
    .dw 0                      ; parent-owned service entry in4000..7FFF
    .db 0,0,0                  ; saved child prefix, result, IFF
    .ds 5
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
.org 0xfda0
worker_gate::
    push bc
    push hl
    push ix
    push iy
    exx
    push bc
    push de
    push hl
    exx
    ex af,af'
    push af
    ex af,af'
    push af
    ld a,i
    di
    ld a,#0
    jp po,worker_iff
    inc a
worker_iff:
    ld (0xfd5a),a
    pop af
    ld c,a
    ld a,#0x40
    in a,(0xf1)
    ld (0xfd58),a
    ld a,(0xfd50)
    cp #'Z'
    jr nz,worker_invalid
    ld a,(0xfd51)
    cp #'W'
    jr nz,worker_invalid
    ld a,(0xfd52)
    cp #'R'
    jr nz,worker_invalid
    ld a,(0xfd53)
    cp #'K'
    jr nz,worker_invalid
    ld a,(0xfd54)
    cp #1
    jr nz,worker_invalid
    ld a,(0xfd55)
    cp #1
    jr nz,worker_invalid
    ld ix,(0xfd56)
    push ix
    pop hl
    ld a,h
    cp #0x40
    jr c,worker_invalid
    cp #0x80
    jr nc,worker_invalid
    ld a,(0xfc09)
    cp #32
    jr c,worker_invalid
    cp #64
    jr nc,worker_invalid
    ld hl,#0
    add hl,sp
    out (0xf1),a
    jp (ix)
worker_invalid:
    ld c,#6
    ld de,#0xffff
    jp worker_return
.org 0xfe40
worker_return::
    ld a,c
    ld (0xfd59),a
    ld a,(0xfd58)
    out (0xf1),a
    ex af,af'
    pop af
    ex af,af'
    exx
    pop hl
    pop de
    pop bc
    exx
    pop iy
    pop ix
    pop hl
    pop bc
    ld a,(0xfd5a)
    or a
    jr z,worker_disabled
    ld a,(0xfd59)
    ei
    ret
worker_disabled:
    ld a,(0xfd59)
    ret
worker_end::
