.module examples_probe
.globl _test_eof_return
.area _TEXT
; Only the supervisor calls this after the guest exits on physical Ctrl-Z.
_test_eof_return::
    ret
