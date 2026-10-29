.module execute_probe
.globl _probe_copy,_probe_call,_probe_data_page,_probe_entry,_test_machine
.area _TEXT
_probe_data_page::
    ld a,#0x80
    in a,(0xf2)
    ret
_probe_copy::
    ld c,a
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,c
    out (0xf2),a
    ld hl,#0x4000
    ld de,#0x8000
    ld bc,#0x4000
    ldir
    ld a,#0xc3
    ld (0x8000),a
    ld hl,#_probe_entry
    ld (0x8001),hl
    pop af
    out (0xf2),a
    ret
_probe_call::
    push ix
    push iy
    ld ix,#0x1357
    ld iy,#0x2468
    exx
    ld bc,#0x1122
    ld de,#0x3344
    ld hl,#0x5566
    exx
    ld hl,#0xf200
    call _test_machine
    ld hl,#0
    xor a
    call 0xfe90
    ld hl,#0xf207
    call _test_machine
    ld (0xf210),ix
    ld (0xf212),iy
    exx
    ld (0xf214),bc
    ld (0xf216),de
    ld (0xf218),hl
    exx
    pop iy
    pop ix
    ret
