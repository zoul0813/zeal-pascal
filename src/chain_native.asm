.module chain_native
.globl _chain_page_io,_chain_buffer,_chain_store
.area _TEXT
; A=owned page, DE=offset, bytes on stack (SDCC call1 third word).
; The entire caller data/stack remains mapped except during this bounded copy.
_chain_page_io::
    ld c,a
    ld a,#0x80
    in a,(0xf2)
    push af
    ld a,c
    out (0xf2),a
    set 7,d
    ld hl,#_chain_buffer
    ld a,(_chain_store)
    or a
    jr nz,copy_ready
    ex de,hl
copy_ready:
    push hl
    ld hl,#6
    add hl,sp
    ld c,(hl)
    inc hl
    ld b,(hl)
    pop hl
    ldir
    pop af
    out (0xf2),a
    pop hl
    inc sp
    inc sp
    jp (hl)
