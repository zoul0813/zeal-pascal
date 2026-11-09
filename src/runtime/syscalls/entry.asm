.module zos_entry
.globl _zs_entry,_zs_dispatch,_zs_invoke
.area _ZOS_ENTRY
; HL names an invocation-owned job; Pascal state survives the C boundary.
_zs_entry::
    push ix
    push iy
    ex af,af'
    push af
    ex af,af'
    exx
    push bc
    push de
    push hl
    exx
    call _zs_dispatch
    exx
    pop hl
    pop de
    pop bc
    exx
    ex af,af'
    pop af
    ex af,af'
    pop iy
    pop ix
    ret
.area _ZOS_INVOKE
; SDCC: HL=register record, E=call. Capture outputs before restoring IX/IY.
; EXEC may destroy every register except HL. Keep the record address on stack.
_zs_invoke::
    push ix
    push iy
    push hl
    ld a,e
    push af
    push hl
    pop ix
    ld c,2(ix)
    ld b,3(ix)
    ld e,4(ix)
    ld d,5(ix)
    ld h,1(ix)
    pop af
    ld l,a
    ld a,0(ix)
    rst 8
    push af
    push bc
    push de
    ld hl,#6
    add hl,sp
    ld e,(hl)
    inc hl
    ld d,(hl)
    push de
    pop ix
    pop de
    pop bc
    pop af
    ld 0(ix),a
    ld 2(ix),c
    ld 3(ix),b
    ld 4(ix),e
    ld 5(ix),d
    pop hl
    pop iy
    pop ix
    ret
