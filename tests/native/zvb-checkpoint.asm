.module zvb_checkpoint
.globl _storage_checkpoint
.area _TEXT
; A resident checkpoint cannot alias hot code at the same banked address.
_storage_checkpoint::
    nop
    ret
