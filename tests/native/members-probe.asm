.module members_probe
.globl _test_phase_restore_marker
.area _TEXT
; Read-only debugger synchronization: callers retain a real C stack.
_test_phase_restore_marker::
    ret

.globl _test_release_retry, _test_release_retry_done
.globl _code_release, _fflush_stdout, _exit
; Always retained: retry frees the test caller's BANK1, so never return there.
_test_release_retry::
    call _code_release
_test_release_retry_done::
    nop
    call _fflush_stdout
    xor a
    jp _exit
