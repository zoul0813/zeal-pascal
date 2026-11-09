.module execute_entry
.globl execute_entry_start,execute_entry_end
.globl execute_entry_probe,execute_entry_release
.area _HEADER (ABS)
.org 0x8000
; Position independent normal-entry template. Only native startup's BC/DE argv
; are live; no return into the old program or unowned retained code is possible.
; Record layout is pinned independently against execute_handoff.h by tests.
execute_entry_start::
    ld sp,#0xfc00
    push bc
    push de
    ld hl,(0xfc00)
    ld de,#0x455a             ; ZE
    or a
    sbc hl,de
    jr nz,cold
    ld hl,(0xfc02)
    ld de,#0x3158             ; X1
    or a
    sbc hl,de
    jr z,present
cold:
    xor a
    ld (0xfc00),a
    pop de
    pop bc
    jp 0x40a0
present:
    ; Magic can be left in arbitrary cold-start RAM. A fresh OS child owns
    ; only its three mapped pages. An owned, unmapped page identifies ending
    ; transfer state; retiring that page is safe even if its record is damaged.
    ld a,(0xfc2a)
    cp #32
    jr c,cold
    cp #64
    jr nc,cold
    ld d,a
    ld a,#0x40
    in a,(0xf1)
    cp d
    jr z,cold
    ld a,#0x80
    in a,(0xf2)
    cp d
    jr z,cold
    ld a,#0xc0
    in a,(0xf3)
    cp d
    jr z,cold
execute_entry_probe::
    ld b,d
    ld l,#26
    rst 8
    or a
    jr z,active
    cp #6                    ; pfree rejects a page not owned by this program
    jr z,cold
    ld hl,#0x010f
    rst 8
probe_stopped:
    jr probe_stopped
active:
    ld hl,#0xfc00
    ld b,#72
    xor a
checksum:
    xor (hl)
    inc hl
    djnz checksum
    or a
    jr nz,bad_header
    ld a,(0xfc04)
    cp #4
    jr c,bad_header
    cp #30
    jr nc,bad_header
    ld hl,(0xfc08)
    ld de,(0x400a)
    or a
    sbc hl,de
    jr nz,bad_header
    ld hl,#0xfc0a
    ld b,#32
reserved:
    ld a,(hl)
    or a
    jr nz,bad_header
    inc hl
    djnz reserved
    ld hl,#0xfc05
    ld a,#0x40
    in a,(0xf1)
    cp (hl)
    jr nz,bad_header
    inc hl
    ld a,#0x80
    in a,(0xf2)
    cp (hl)
    jr nz,bad_header
    inc hl
    ld a,#0xc0
    in a,(0xf3)
    cp (hl)
    jr z,owners
bad_header:
    xor a
    ld (0xfc00),a
    ld hl,#0x010f
    rst 8
header_stopped:
    jr header_stopped
owners:
    ld a,(0xfc04)
    ld c,a
    ld hl,#0xfc2a
owner:
    ld a,(hl)
    cp #32
    jr c,bad_owners
    cp #64
    jr nc,bad_owners
    ld d,a
    ld a,(0xfc05)
    cp d
    jr z,bad_owners
    ld a,(0xfc06)
    cp d
    jr z,bad_owners
    ld a,(0xfc07)
    cp d
    jr z,bad_owners
    ld a,(0xfc04)
    sub c
    ld b,a
    or a
    jr z,unique
    ld iy,#0xfc2a
previous:
    ld a,0(iy)
    cp d
    jr z,bad_owners
    inc iy
    djnz previous
unique:
    inc hl
    dec c
    jr nz,owner
    jr release
bad_owners:
    xor a
    ld (0xfc00),a
    ld hl,#0x010f
    rst 8
owners_stopped:
    jr owners_stopped
release:
execute_entry_release::
    xor a
    ld (0xfc00),a             ; full record validated before any other owner free
    ld ix,#0xfc2b
    ld a,(0xfc04)
    dec a
    ld c,a
    ld b,#0
free_owner:
    push bc
    ld b,0(ix)
    ld l,#26
    rst 8
    pop bc
    or a
    jr z,freed
    ld b,#1                  ; retain failure while attempting every other owner
freed:
    inc ix
    dec c
    jr nz,free_owner
    ld a,b
    or a
    jr nz,bad_release
    pop de
    pop bc
    jp 0x40a0
bad_release:
    ld hl,#0x010f
    rst 8
release_stopped:
    jr release_stopped
execute_entry_end::
