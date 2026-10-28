.module test_file_gate
.globl _gate_before,_gate_after,_gate_registers,_test_machine
.area _TEXT
_test_parent_page::
    ld a,#0x40
    in a,(0xf1)
    ret
_test_module_copy::
    ld c,a
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,c
    out (0xf2),a
    ld hl,#fake_start
    ld de,#0x8000
    ld bc,#fake_end-fake_start
    ldir
    pop af
    out (0xf2),a
    ret
; A=mode, DE=job. Compare observations at identical stack depth, and poison
; the fake callee's index/alternate sets to exercise the gate's real saves.
_test_gate_call::
    push ix
    push iy
    push de
    push af
    ld ix,#0x1357
    ld iy,#0x2468
    exx
    ld bc,#0x1122
    ld de,#0x3344
    ld hl,#0x5566
    exx
    ld hl,#0x7744
    push hl
    pop af
    ex af,af'
    ld hl,#_gate_before
    call _test_machine
    pop af
    pop hl
    call 0xfe90
    push af
    push hl
    ld hl,#_gate_after
    call _test_machine
    pop hl
    pop af
    ld (_gate_registers),ix
    ld (_gate_registers+2),iy
    exx
    ld (_gate_registers+4),bc
    ld (_gate_registers+6),de
    ld (_gate_registers+8),hl
    exx
    ex af,af'
    push af
    pop hl
    ld (_gate_registers+10),hl
    ex af,af'
    pop iy
    pop ix
    ret
fake_start:
    .db 0xc3,0x10,0x40,0xc3,0x10,0x40
    .ds 10
    ; Both callable entries land at4010. E000 supplies the simulated retained
    ; page count; E001 counts actual dispatches across retry and prefix swaps.
    .db 0x21,0x01,0xe0,0x34
    .db 0xdd,0x21,0x34,0x12,0xfd,0x21,0x78,0x56
    .db 0xd9,0x01,0xaa,0xbb,0x11,0xcc,0xdd,0x21,0xee,0xff,0xd9
    .db 0x08,0x3e,0x99,0x08,0x3a,0x00,0xe0,0xc9
fake_end:
