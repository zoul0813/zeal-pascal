.module execute_native
.globl _execute_code_page,_execute_stack_page
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
