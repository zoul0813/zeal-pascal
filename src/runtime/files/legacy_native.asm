.module file_native
.globl _zf_service,_zf_service_value
.area _TEXT
_zf_service::                 ; A=operation, DE=argument, result A/native DE
    call 0xfda0
    ld (_zf_service_value),de
    ret
