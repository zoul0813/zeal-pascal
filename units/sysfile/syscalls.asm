; Zeal OS syscall adapters (Apache-2.0, Zeal 8-bit Computer)
; Maintained Z80 assembly. Initially migrated from the verified native backend.
; File semantics remain specified by src/runtime/files/operations.c and its tests.

.macro syscall number
    ld l,#number
    rst 8
.endm
STD_BUFFER_SIZE = 80
fragment f_read
_read:
    pop hl
    ex (sp), hl
    ld c, (hl)
    inc hl
    ld b, (hl)
    push hl
    ld h, a
    syscall 0
    pop hl
    or a
    ret nz
    ld (hl), b
    dec hl
    ld (hl), c
    ret
endfragment f_read

fragment f_write
_write:
    pop hl
    ex (sp), hl
    ld c, (hl)
    inc hl
    ld b, (hl)
    push hl
    ld h, a
    syscall 1
    pop hl
    or a
    ret nz
    ld (hl), b
    dec hl
    ld (hl), c
    ret
endfragment f_write

fragment f_open
_open:
    ld b, h
    ld c, l
    pop hl
    dec sp
    ex (sp), hl
    syscall 2
    ret
endfragment f_open

fragment f_close
_close:
    ld h, a
    syscall 3
    ret
endfragment f_close

fragment f_stat
_stat:
    ld b, h
    ld c, l
    syscall 5
    ret
endfragment f_stat

fragment f_seek
_seek:
    pop hl
    dec sp
    ex (sp), hl
    ld l, a
    ld a, h
    ld h, l
    push hl
    ex de, hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    inc hl
    ld c, (hl)
    inc hl
    ld b, (hl)
    ex (sp), hl
    syscall 6
    pop hl
    or a
    ret nz
    ld (hl), b
    dec hl
    ld (hl), c
    dec hl
    ld (hl), d
    dec hl
    ld (hl), e
    ret
endfragment f_seek

fragment f_ioctl
_ioctl:
    ld c, l
    pop hl
    ex (sp), hl
    ex de, hl
    ld h, a
    syscall 7
    ret
endfragment f_ioctl

fragment f_curdir
_curdir:
    ex de, hl
    syscall 10
    ret
endfragment f_curdir

fragment f_rm
_rm:
    ex de, hl
    syscall 13
    ret
endfragment f_rm
