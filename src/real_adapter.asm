.module real_adapter
.globl real_dispatch,real_buffer,call_error
.globl _zr_eval,_zr_op,_zr_error,_zr_integer,_zr_a,_zr_b,_zr_result
.globl format_real,_zr_width,_zr_places,_zr_formatted
.globl val_dispatch,_zr_text,_zr_length,_zr_code,_zr_assign
.area _REAL_DATA
real_left: .ds 2
real_right: .ds 2
real_flags: .ds 1
real_status: .ds 1
.area _REAL_ADAPTER
; HL owned short string, A Val mode. Reply: A assignment flag, DE Code,
; HL Integer or six-byte result pointer, carry set for Integer. Code failures
; do not enter the fatal arithmetic path. The caller retains both destinations.
val_dispatch::
    ld (real_flags),a
    ld a,(hl)
    ld (_zr_length),a
    xor a
    ld (_zr_length+1),a
    inc hl
    ld (_zr_text),hl
    jr real_save
; Pure formatting uses the same preserved-register C boundary as arithmetic.
; HL is a six-byte value, BC width and DE places. No callback occurs here.
format_real::
    ld (_zr_width),bc
    ld (_zr_places),de
    ld d,h
    ld e,l
    ld a,#14
    jp real_dispatch
; Private generated-code interface. Bits6/7 mark Integer words rather than
; Real pointers. Real results are copied immediately out of C scratch storage.
; The caller reserves256 plus the normal64-byte helper/emergency budget.
real_dispatch::
    ld (real_left),de
    ld (real_right),hl
    ld (real_flags),a
real_save:
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
    ld a,(real_flags)
    and #63
    cp #15
    jr z,real_evaluate
    cp #16
    jr z,real_evaluate
    cp #5                       ; Integer -> Real needs no pointer operands
    jr z,real_from_integer
    ld a,(real_flags)
    bit 6,a
    jr z,real_copy_left
    ld hl,(real_left)
    ld (_zr_integer),hl
    ld a,#5
    ld (_zr_op),a
    call _zr_eval
    ld hl,#_zr_result
    jr real_store_left
real_copy_left:
    ld hl,(real_left)
real_store_left:
    ld de,#_zr_a
    ld bc,#6
    ldir
    ld a,(real_flags)
    bit 7,a
    jr z,real_copy_right
    ld hl,(real_right)
    ld (_zr_integer),hl
    ld a,#5
    ld (_zr_op),a
    call _zr_eval
    ld hl,#_zr_result
    jr real_store_right
real_copy_right:
    ld hl,(real_right)
real_store_right:
    ld de,#_zr_b
    ld bc,#6
    ldir
    jr real_evaluate
real_from_integer:
    ld hl,(real_right)
    ld (_zr_integer),hl
real_evaluate:
    ld a,(real_flags)
    and #63
    ld (_zr_op),a
    call _zr_eval
    ld a,(_zr_error)
    ld (real_status),a
    ld hl,#_zr_result
    ld de,#real_buffer
    ld bc,#6
    ldir
    ld hl,(_zr_integer)
    ld (real_right),hl
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
    ld a,(real_status)
    or a
    jr z,real_success
    ld b,#2
    jp call_error
real_success:
    ld a,(real_flags)
    and #63
    cp #15
    jr z,real_val
    cp #16
    jr z,real_val
    cp #14
    jr z,real_formatted
    cp #4
    jr z,real_scalar
    cp #6
    jr z,real_scalar
    cp #7
    jr z,real_scalar
    ld hl,#real_buffer
    ret
real_formatted:
    ld hl,#_zr_formatted
    ret
real_scalar:
    ld hl,(real_right)
    ret
real_val:
    cp #16
    ld hl,#real_buffer
    jr nc,real_val_reply
    ld hl,(real_right)
real_val_reply:
    ld de,(_zr_code)
    ld a,(_zr_assign)
    ret
