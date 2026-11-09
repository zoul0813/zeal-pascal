; Pascal register boundary
; Maintained Z80 assembly. Initially migrated from the verified native backend.
; File semantics remain specified by src/runtime/files/operations.c and its tests.

fragment f_zf_entry
_zf_entry:
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
    reference r752, f_zf_entry, 1, f_zf_dispatch, f_zf_dispatch, 0
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
endfragment f_zf_entry

fragment f_zf_operations_address
_zf_operations_address:
    reference r753, f_zf_operations_address, 1, sf_data, _zf_operations, 0
    ld hl,#_zf_operations
    ret
endfragment f_zf_operations_address
