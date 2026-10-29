.module transfer_stop
.globl _transfer_parent_stop
.area _STOP (ABS)
.org 0x7ff0
; This observer cannot alias executable runtime/file/transfer code in a child.
_transfer_parent_stop::
    nop
    jp _transfer_parent_stop
