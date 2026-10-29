.module execute_parent_stop
.globl _execute_parent_stop
.area _STOP (ABS)
.org 0x7ff0
; Keep the supervisor checkpoint outside child runtime/loader executable code.
_execute_parent_stop::
    nop
    jp _execute_parent_stop
