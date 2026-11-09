.module transfer_module
.globl _transfer_dispatch,_transfer_cleanup,_transfer_file_gate,_transfer_file_gate_bytes
.globl transfer_dispatch_entry,transfer_cleanup_entry
.area _HEADER
    jp transfer_dispatch_entry
    jp transfer_cleanup_entry
    .dw _transfer_file_gate,_transfer_file_gate_bytes
    .ascii "ZTRN"
    .db 1,3
.area _SYSTEM
.area _TEXT
transfer_dispatch_entry:
    ld (transfer_saved_sp),sp
    ld sp,#0x8000
    call _transfer_dispatch
    ld sp,(transfer_saved_sp)
    ret
transfer_cleanup_entry:
    ld (transfer_saved_sp),sp
    ld sp,#0x8000
    call _transfer_cleanup
    ld sp,(transfer_saved_sp)
    ret
.area _INITIALIZED
.area _DATA
transfer_saved_sp: .ds 2
.area _BSS
