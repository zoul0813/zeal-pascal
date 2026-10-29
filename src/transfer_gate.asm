.module transfer_gate
.globl transfer_gate_end,transfer_gate_returned
.area _HEADER (ABS)
.org 0xfc80
; A=1 scoped file cleanup,2 terminal composite cleanup,3 transfer dispatch.
; FE75 owns the coordinator; FE77 temporarily preserves the real file module.
    ld b,a
    cp #1
    jr z,file_only
    ld a,(0xfe75)
    or a
    jr nz,coordinator
    ld a,b
    cp #3
    jr nz,file_only
    ld de,#50                 ; absent coordinator is a failed transfer job
    add hl,de
    ld (hl),#0x20
    inc hl
    ld (hl),#0
    inc hl
    ld (hl),#0
    inc hl
    ld (hl),#0
    xor a
    ld (0xfe7f),a
    ret
coordinator:
    ld a,b
    ld (0xfe79),a
    push hl
    ld a,(0xfe75)
    out (0xf1),a
    ld hl,(0x4006)
    ld bc,(0x4008)
    ld de,#0xfe90
    ldir                      ; restore our qualified nested transport template
    ld a,(0xfe76)
    out (0xf1),a
    pop hl
    ld a,(0xfe74)
    ld (0xfe77),a
    ld a,(0xfe75)
    ld (0xfe74),a
    ld a,(0xfe79)
    cp #3
    ld a,#2
    jr nz,transport
    xor a
transport:
    call 0xfe90
    ld a,(0xfe74)
    ld (0xfe75),a
    ld a,(0xfe77)
    ld (0xfe74),a
transfer_gate_returned::
    ret
file_only:
    ld a,(0xfe74)
    or a
    jr nz,file_live
    ld (0xfe7f),a
    ret
file_live:
    ld a,b
    jp 0xfe90
transfer_gate_end::
