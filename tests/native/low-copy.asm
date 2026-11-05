.module low_copy_probe
.globl _test_machine,_zf_low_copy,_low_before,_low_after,_low_status,_low_registers,_low_arg
.area _TEXT
_low_invoke::
    push ix
    push hl
    ld hl,#_low_before
    call _test_machine
    pop hl
    ld (_low_arg),hl
    ld ix,#0x1357
    ld bc,#0x2468
    ld de,#0x579b
    call _zf_low_copy
    ld (_low_status),a
    ld (_low_registers),ix
    ld (_low_registers+2),bc
    ld (_low_registers+4),de
    ld (_low_registers+6),hl
    push hl
    ld hl,#_low_after
    call _test_machine
    pop hl
    pop ix
    ld a,(_low_status)
    ret
; Independent setup/oracle, never use the bridge under test to verify itself.
_low_seed::
    ld c,a
    ld a,i
    push af
    di
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,c
    out (0xf2),a
    ld hl,#0x8000
    ld de,#0x8001
    ld bc,#0x3fff
    ld (hl),#0xa5
    ldir
    jr restore
_low_peek::
    ld a,i
    push af
    di
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,(0xfe76)
    out (0xf2),a
    set 7,h
    ld b,(hl)
restore:
    pop af
    out (0xf2),a
    pop af
    jp po,disabled
    ei
disabled:
    ld a,b
    ret

_low_bad_stack::
    ld a,i
    push af
    di
    ld (old_sp),sp
    ld sp,#0xbff8
    call _zf_low_copy
    ld (_low_status),a
    ld sp,(old_sp)
    pop af
    jp po,bad_disabled
    ei
bad_disabled:
    ld a,(_low_status)
    ret
_low_stack_reject::
    ld de,#0xc00d              ; helper SP after its saves is C003
    jr edge_stack
_low_stack_accept::
    ld de,#0xc00e              ; helper SP after its saves is C004
    jr edge_stack
_low_stack_upper::
    ld de,#0x8001              ; return frame crosses page1 into page2
edge_stack:
    ld a,i
    push af
    di
    push hl
    push de
    ld hl,#_low_before
    call _test_machine
    pop de
    pop hl
    ld (old_sp),sp
    ex de,hl
    ld sp,hl
    ex de,hl
    call _zf_low_copy
    ld (_low_status),a
    ld sp,(old_sp)
    push hl
    push de
    ld hl,#_low_after
    call _test_machine
    pop de
    pop hl
    pop af
    jp po,edge_disabled
    ei
edge_disabled:
    ld a,(_low_status)
    ret
.area _DATA
old_sp: .ds 2
