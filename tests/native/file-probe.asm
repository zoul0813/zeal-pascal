.module file_stack_probe
.globl _file_probe,_file_probe_before,_file_probe_after,_file_probe_ok
.globl _test_machine,_zf_dispatch,_zf_cleanup
.area _TEST
; A=cleanup selector, DE=caller job. Both observations have exactly the same
; call depth. A32-byte canary at514..545 bytes below that entry checks a
; conservative512-byte component budget, including SDK/syscall activity.
; This is a test caller's ordinary stack below FC00, never transition data.
_file_probe::
    push de
    push af
    ld hl,#-544
    add hl,sp
    ld b,#32
fill:
    ld (hl),#0xa5
    inc hl
    djnz fill
    ld hl,#_file_probe_before
    call _test_machine
    pop af
    pop hl
    or a
    jr nz,cleanup
    call _zf_dispatch
    jr observed
cleanup:
    call _zf_cleanup
observed:
    push af
    push af
    ld hl,#_file_probe_after
    call _test_machine
    pop af
    pop af
    ld hl,#-548
    add hl,sp
    ld b,#32
verify:
    ld a,(hl)
    cp #0xa5
    jr nz,failed
    inc hl
    djnz verify
    ld a,#1
    jr reply
failed:
    xor a
reply:
    ld (_file_probe_ok),a
    ret
