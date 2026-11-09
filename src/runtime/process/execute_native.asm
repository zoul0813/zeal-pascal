.module execute_native
.globl _execute_code_page,_execute_stack_page
.globl _execute_switch,_execute_attempt,_execute_os_override,_execute_abandon
.globl execute_stack_returned,execute_os_returned
.globl execute_caller_stack,execute_saved_sp,execute_saved_stack,execute_saved_iff
.area _TEXT
; This component executes in the retained4000 prefix.
_execute_code_page::
    ld a,#0x40
    in a,(0xf1)
    ret
; Query the actual stack window, including a future retained-prefix alias.
_execute_stack_page::
    ld hl,#0
    add hl,sp
    ld a,h
    and #0xc0
    jr z,stack0
    cp #0x40
    jr z,stack1
    cp #0x80
    jr z,stack2
    in a,(0xf3)
    ret
stack2:
    in a,(0xf2)
    ret
stack1:
    in a,(0xf1)
    ret
stack0:
    in a,(0xf0)
    ret

; Freeze the caller's frames before snapshot. The retained prefix is aliased
; atC000, with a private stack topFC00 and1024 bytes of reserved headroom.
; No original page is mapped as code or stack during snapshot/override/restore.
_execute_switch::
    push ix
    push iy
    ld a,i
    di
    jp po,was_disabled
    ld a,#1
    jr save_iff
was_disabled:
    xor a
save_iff:
    ld (execute_saved_iff),a
    ld (execute_saved_sp),sp
    ld a,#0xc0
    in a,(0xf3)
    ld (execute_saved_stack),a
    ld a,#0x40
    in a,(0xf1)
    out (0xf3),a
    ld sp,#0xfc00
    ld a,(execute_saved_iff)
    or a
    jr z,attempt
    ei
attempt:
    call _execute_attempt
execute_stack_returned::
    di
    ld a,(execute_saved_stack)
    out (0xf3),a
    ld sp,(execute_saved_sp)
    pop iy
    pop ix
execute_caller_stack::
    ld a,(execute_saved_iff)
    or a
    ret z
    ei
    ret

; Supported OS override ABI, no argv. Success jumps into the new program;
; failure returns to the retained mapping/stack saved by the dispatcher.
_execute_os_override::
    ld b,h
    ld c,l
    ld de,#0
    ld hl,#16
    rst 8
execute_os_returned::
    ret
_execute_abandon::
    ld hl,#0x010f
    rst 8
abandoned:
    jr abandoned
.area _DATA
execute_saved_sp:: .ds 2
execute_saved_stack:: .ds 1
execute_saved_iff:: .ds 1
