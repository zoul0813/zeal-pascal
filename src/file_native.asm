.module file_native
.globl _zf_record_io,_zf_page_clear,_zf_service
.globl _zf_record,_zf_store,_zf_service_value
.area _TEXT
; No callbacks/syscalls occur with page2 replaced. Prefix code and scratch
; live together in the owned module page; caller code at8000 is restored
; before the RET. Primary registers are scratch under the C ABI.
_zf_record_io::                ; A=page, DE=offset
    ld c,a
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,c
    out (0xf2),a
    set 7,d
    ld hl,#_zf_record
    ld a,(_zf_store)
    or a
    jr nz,record_ready
    ex de,hl
record_ready:
    ld bc,#160
    ldir
    pop af
    out (0xf2),a
    ret
_zf_page_clear::               ; A=page, explicitly clear native dirty RAM
    ld c,a
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,c
    out (0xf2),a
    ld hl,#0x8000
    ld de,#0x8001
    ld bc,#0x3fff
    ld (hl),#0
    ldir
    pop af
    out (0xf2),a
    ret
_zf_service::                 ; A=operation, DE=argument, result A/native DE
    call 0xfda0
    ld (_zf_service_value),de
    ret
