.module members_probe
.globl _test_phase_restore_marker
.area _TEXT
; Read-only debugger synchronization: callers retain a real C stack.
_test_phase_restore_marker::
    ret
