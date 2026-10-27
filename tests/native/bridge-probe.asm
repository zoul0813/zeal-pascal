.module bridge_probe
.globl _bridge_guard_marker
.area _BRIDGE_MARKER (ABS)
.org 0x7fb0
; This is data/scratch, never executable code, in every Pascal prefix and file
; module. A virtual breakpoint here cannot mistake a child helper for the parent.
_bridge_guard_marker::
    ret
