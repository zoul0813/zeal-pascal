.module native
.globl _far_get, _far_set, _far_read, _ram_enter
.globl _ram_result, _ram_pages, _ram_entry
.globl _ram_io_detail,_ram_cleanup_detail
.globl _console_scratch
.globl _key_raw
.globl _page_versions
.globl _ram_worker_service, _worker_dispatch, _code_pages
.globl _worker_child_page, _worker_child_prefix, _worker_child_page2, _worker_value

; Retained low-memory core. The link-map acceptance test checks the entire
; area (including control data) remains in page 1. C code/literals may use
; page2; span callers retain their stack in page3.
.area _HOME
_far_get::                       ; A=owned physical page, DE=bounded offset
    ld c,a
    ld a,#0x80
    in a,(0xf2)
    ld b,a
    ld a,c
    out (0xf2),a
    set 7,d
    ld a,(de)
    ld c,a
    ld a,b
    out (0xf2),a
    ld a,c
    ret
_far_set::                       ; A=page, DE=offset, byte on stack
    ld c,a
    pop hl
    dec sp
    ex (sp),hl                   ; H=value; return word remains at SP
    ; Every store to a far page changes its version, even identical writes.
    ; Keep these counters in retained page 1 with the mapping core.
    push hl
    push de
    ld l,c
    ld h,#0
    add hl,hl
    add hl,hl
    ld de,#_page_versions
    add hl,de
    inc (hl)
    jr nz,version_done
    inc hl
    inc (hl)
    jr nz,version_done
    inc hl
    inc (hl)
    jr nz,version_done
    inc hl
    inc (hl)
version_done:
    pop de
    pop hl
    ld a,#0x80
    in a,(0xf2)
    ld b,a
    ld a,c
    out (0xf2),a
    set 7,d
    ld a,h
    ld (de),a
    ld a,b
    out (0xf2),a
    ret

; Checked span read. A=owned RAM page, DE=offset; out/count are LE16 stack
; arguments under SDCC call1. IX/IY and caller SP/IFF are preserved. Bounds
; reject before mapping/stores; zero is a no-op even with sentinel arguments.
_far_read::
    push ix
    ld ix,#0
    add ix,sp
    push af                      ; page at -1(IX)
    push de                      ; offset at -4(IX)
    ld a,i
    di
    push af                      ; incoming IFF in P/V, retained until exit
    ld c,6(ix)
    ld b,7(ix)
    ld a,b
    or c
    jr z,read_success            ; never enter LDIR with BC=0
    ld a,b
    cp #1
    jr c,read_count_valid
    jr nz,read_invalid
    ld a,c
    or a
    jr nz,read_invalid           ; FAR_READ_LIMIT=256 bounds IRQ masking
read_count_valid:
    ld a,-1(ix)
    cp #32
    jr c,read_invalid
    cp #64
    jr nc,read_invalid
    ld h,d
    ld l,e
    add hl,bc                    ; exclusive source end, including carry
    jr c,read_invalid
    ld a,h
    cp #0x40
    jr c,read_source_valid
    jr nz,read_invalid
    ld a,l
    or a
    jr nz,read_invalid
read_source_valid:
    ld l,4(ix)
    ld h,5(ix)
    ld e,l
    ld d,h                       ; DE retains the destination start
    ld a,h
    and #0xc0
    cp #0x40
    jr z,read_near_window
    cp #0xc0
    jr nz,read_invalid
read_near_window:
    dec bc
    add hl,bc                    ; inclusive last byte permits FFFF
    jr c,read_invalid
    xor h
    and #0xc0                    ; start and last must share retained window
    jr nz,read_invalid
    inc bc
    ld l,-4(ix)
    ld h,-3(ix)
    set 7,h
    ld a,#0x80
    in a,(0xf2)
    push af                      ; old page2, never needed across a callback
    ld a,-1(ix)
    out (0xf2),a
    ldir
    pop af
    out (0xf2),a                 ; restore before unmasking interrupts
    jr read_success
read_invalid:
    xor a
    jr read_finish
read_success:
    ld a,#1
read_finish:
    ld c,a
    pop af                       ; recover incoming IFF, not validation flags
    ld a,c
    ld sp,ix
    pop ix
    pop hl                       ; caller return address
    pop bc                       ; consume out/count per SDCC call1 ABI
    pop bc
    jp po,read_return
    ei
read_return:
    jp (hl)

_ram_enter::
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
    ld (_parent_sp),sp
    ld a,i
    di
    ld a,#0
    jp po,iff_saved
    inc a
iff_saved:
    ld (_parent_iff),a
    ld (_ram_result+15),a
    ld hl,(_parent_sp)
    ld (_ram_result+16),hl
    ld hl,#return_core
    ld (_ram_result+18),hl
    ld hl,#_ram_result
    ld (_ram_result+20),hl
    xor a
    in a,(0xf0)
    ld (_ram_result+8),a
    ld a,#0x40
    in a,(0xf1)
    ld (_ram_result+9),a
    ld a,#0x80
    in a,(0xf2)
    ld (_ram_result+10),a
    ld a,#0xc0
    in a,(0xf3)
    ld (_parent_page3),a
    ld (_ram_result+11),a
    ld a,(_ram_pages+2)
    out (0xf3),a
    ld sp,#0xfc00               ; no stack access between OUT and LD SP
    ld hl,#_ram_result
    ld de,#0xfc00
    ld bc,#128
    ldir
    jp 0xfd00
return_core:
    di
    ld hl,#0xfc00
    ld de,#_ram_result
    ld bc,#128
    ldir
    ld hl,#0
    ld a,(_ram_result+26)
    cp #3
    jr nz,detail_copied
    ld a,(_ram_result+27)
    cp #3
    jr nz,detail_copied
    ld hl,(0xfe7a)
detail_copied:
    ld (_ram_io_detail),hl
    ld a,(0xfe73)
    ld (_ram_cleanup_detail),a
    ld a,(_parent_page3)
    out (0xf3),a
    ld sp,(_parent_sp)          ; no stack access until backing is restored
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
    ld a,(_parent_iff)
    or a
    ret z
    ei
    ret

; Called from FDA0 with child page3/stack still mapped. HL is the saved child
; SP, C is the operation, DE its argument. Run the owner's C service on the
; already-suspended parent stack, then restore backing before touching it.
; No child subroutine stack enters FC00..FFFF. Parent SP/frame and return metadata
; remain intact for the eventual ordinary program return. Only the mapping/SP
; transitions mask interrupts; the complete parent service context honors the
; child's incoming IFF so keyboard release bytes can arrive during bookkeeping.
_ram_worker_service::
    ld (_worker_child_sp),hl
    ld a,(0xfd58)
    ld (_worker_child_prefix),a
    ld a,(0xfd5a)              ; capture before replacing child page3
    ld (_worker_child_iff),a
    ld a,#0x80
    in a,(0xf2)
    ld (_worker_child_page2),a
    ld a,#0xc0
    in a,(0xf3)
    ld (_worker_child_page),a
    ld a,(_parent_page3)
    out (0xf3),a
    ld sp,(_parent_sp)
    ld a,(_code_pages+2)       ; ordinary file/service bank in parent page2
    out (0xf2),a
    ld a,(_worker_child_iff)
    or a
    jr z,worker_service_ready
    ei
worker_service_ready:
    ld a,c
    call _worker_dispatch
    di
    ld c,a
    ld de,(_worker_value)
    ld a,(_worker_child_page2)
    out (0xf2),a
    ld a,(_worker_child_page)
    out (0xf3),a
    ld sp,(_worker_child_sp)
    jp 0xfe40                 ; restore prefix from code outside that window
_worker_child_sp: .ds 2
_worker_child_iff: .ds 1
_worker_child_page:: .ds 1
_worker_child_prefix:: .ds 1
_worker_child_page2:: .ds 1
_worker_value:: .ds 2
_parent_sp: .ds 2
_parent_iff: .ds 1
_parent_page3: .ds 1
_ram_entry:: .ds 2
_ram_pages:: .ds 3
_ram_result:: .ds 128
_ram_io_detail:: .ds 2
_ram_cleanup_detail:: .ds 1
_console_scratch:: .ds 256
_key_raw:: .ds 32
_page_versions:: .ds 256
