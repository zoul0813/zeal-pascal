.module file_direct
.globl _zf_dispatch,_zf_operations,_zf_entry,_zf_operations_address
.area _FILE_GATE
; The caller's normal mapping and stack remain active. Preserve Pascal's
; lexical/With and alternate register state across the private SDCC boundary.
_zf_entry::
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
    call _zf_dispatch
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
.area _FILE_TABLE
; A compiler-private symbol anchor. Its relocated operand names the table.
_zf_operations_address::
    ld hl,#_zf_operations
    ret
