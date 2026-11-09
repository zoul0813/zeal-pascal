.module file_module
.globl _zf_dispatch,_zf_cleanup,file_dispatch_entry,file_cleanup_entry
.area _HEADER
    jp file_dispatch_entry
    jp file_cleanup_entry
    .ascii "ZFIL"
    .db 1,0
    .dw 16,0
.area _SYSTEM
.area _TEXT
; The retained gate has saved all caller registers on the Pascal stack.
; Native C uses this module's spare tail, outside the Pascal recursion split.
file_dispatch_entry:
    ld (file_saved_sp),sp
    ld sp,#0x8000
    call _zf_dispatch
    ld sp,(file_saved_sp)
    ret
file_cleanup_entry:
    ld (file_saved_sp),sp
    ld sp,#0x8000
    call _zf_cleanup
    ld sp,(file_saved_sp)
    ret
.area _INITIALIZED
.area _DATA
file_saved_sp: .ds 2
.area _BSS
