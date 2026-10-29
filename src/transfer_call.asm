.module transfer_call
.globl transfer_call_start,transfer_call_end,transfer_call_relocs,transfer_call_relocs_end
.globl transfer_call_module_operand,transfer_call_gate_operand,transfer_call_file_operand
.globl transfer_call_input_operand,zt_profile,zt_module_size,zt_gate_size
.area _HEADER (ABS)
.org 0x8000
; HL is the once-evaluated ordinary FileJob. The compiler reserves256 bytes
; before entry; every C service additionally has the active1024-byte budget.
transfer_call_start::
    ld a,(0x4164)
    or a
    jp nz,0x415e
    ld a,(0xfe78)
    or a
    jr z,native
    ld a,#21
    jp 0x415e                 ; before path I/O or coordinator acquisition
native:
    push iy
    push hl
    ld hl,#-182               ;54-byte transfer job followed by128-byte path
    add hl,sp
    ld sp,hl
    ld hl,#182
    add hl,sp
    ld e,(hl)
    inc hl
    ld d,(hl)
    push de
    pop iy                    ; original FileJob, old lexical IX unchanged
    ld hl,#54
    add hl,sp
    ld 6(iy),l
    ld 7(iy),h
    ld 8(iy),#128
    ld 9(iy),#0
    push iy
    pop hl
    call 0x4153
transfer_call_file_operand::
    .dw 0                     ; compiler patches its ordinary file helper
    .db 0                     ; inner I-; restore With before propagating
    ld a,(0x4164)
    or a
    jr z,path_ready
    ld e,a
    ld d,#0
    .db 0xc3
rel_path: .dw finish
path_ready:
    ld a,(0xfe75)
    or a
    jr nz,module_ready
    ld l,#25
    rst 8                     ; native caller owns its palloc page
    or a
    jr z,allocated
    ld l,a
    ld h,#0
    ld (0xfe7a),hl
    ld de,#0x1ff
    .db 0xc3
rel_alloc: .dw finish
allocated:
    ld a,b
    ld (0xfe75),a
    ld a,#0x40
    in a,(0xf1)
    ld (0xfe76),a
    ld a,(0xfe75)
    out (0xf1),a
    ld hl,#0x4000
    ld de,#0x4001
    ld bc,#0x3fff
    ld (hl),#0
    ldir
    .db 0x21
transfer_call_module_operand:: .dw 0
    ld de,#0x4000
    ld bc,#zt_module_size
    ldir
    ld a,(0xfe76)
    out (0xf1),a
    .db 0x21
transfer_call_gate_operand:: .dw 0
    ld de,#0xfc80
    ld bc,#zt_gate_size
    ldir
module_ready:
    ld hl,#1024
    ld (0xfe7c),hl
    ld hl,#0
    add hl,sp
    push hl
    pop iy                    ; caller-owned TransferJob, exact54-byte shape
    ld 0(iy),#0
    ld 1(iy),#0
    ld hl,(0x4014)
    ld 2(iy),l
    ld 3(iy),h
    ld hl,(0x4016)
    ld 4(iy),l
    ld 5(iy),h
    ld hl,(0x401a)
    ld 6(iy),l
    ld 7(iy),h
    ld hl,(0x401e)
    ld 8(iy),l
    ld 9(iy),h
    ld hl,(0x415c)
    ld 10(iy),l
    ld 11(iy),h
    ld hl,(0x412c)
    ld 12(iy),l
    ld 13(iy),h
    ld hl,#0
    add hl,sp
    ld 14(iy),l
    ld 15(iy),h
    ld de,#16
    add hl,de
    ex de,hl
    .db 0x21
rel_profile: .dw zt_profile
    ld bc,#32
    ldir
    ld hl,#54
    add hl,sp
    ld 48(iy),l
    ld 49(iy),h
    .db 0xcd
rel_prepare: .dw dispatch
    ld a,50(iy)
    or 51(iy)
    jr nz,finish_job
    .db 0xcd
transfer_call_input_operand:: .dw zt_input_cleanup
    or a
    jr z,commit
    push af                   ; cleanup failure remains primary over discard
    ld 0(iy),#2
    .db 0xcd
rel_discard: .dw dispatch
    pop af
    ld l,a
    ld h,#0
    ld (0xfe7a),hl
    ld de,#0x20
    jr finish
commit:
    ld 0(iy),#1
    .db 0xcd
rel_commit: .dw dispatch
    ld a,50(iy)
    or 51(iy)
    jr nz,finish_job
    ld 50(iy),#0x20           ; a successful commit cannot return
finish_job:
    ld l,52(iy)
    ld h,53(iy)
    ld (0xfe7a),hl
    ld e,50(iy)
    ld d,51(iy)
finish:
    ld hl,#184
    add hl,sp
    ld sp,hl
    pop iy                    ; original With register, before any error jump
    ld a,d
    or a
    jr z,io_result
    ld a,e
    ld b,#2
    jp 0x4161
io_result:
    ld a,e
    or a
    ret z
    jp 0x415e
dispatch:
    push iy
    pop hl
    ld a,#3
    call 0xfc80
    ld a,(0xfe7f)
    or a
    ret z
    ld 52(iy),a
    ld 53(iy),#0
    ld 50(iy),#255
    ld 51(iy),#1
    ret
.include "transfer_call_data.inc"
transfer_call_relocs::
    .dw rel_path,rel_alloc,rel_profile,rel_prepare,rel_discard,rel_commit
transfer_call_relocs_end::
transfer_call_end::
