; Caller-private workspace; zeroed by the ordinary Pascal fragment linker.
.area _ZPDATA
sf_data:
_native_fd:
.rept 1
.db 0
.endm
_job:
.rept 2
.db 0
.endm
_record:
.rept 2
.db 0
.endm
_bytes:
.rept 128
.db 0
.endm
_transport:
.rept 129
.db 0
.endm
_rename_path:
.rept 128
.db 0
.endm
_info:
.rept 29
.db 0
.endm
_native_position:
.rept 4
.db 0
.endm
_serial_attribute:
.rept 1
.db 0
.endm
_zf_operations:
.rept 68
.db 0
.endm
sf_data_end:
zpu_end:
