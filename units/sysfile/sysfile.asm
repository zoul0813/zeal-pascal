; SYSFILE.ZPU is assembled as an ordinary byte stream at address zero.
.module sysfile
; Named adapter symbols also support the test-only SER0 transport probe.
.globl _open,_close,_read,_write,_ioctl
.area _ZPHEADER
.area _ZPEXPORTS
.area _ZPFLAGS
.area _ZPBLOB
.area _ZPFRAGMENTS
.area _ZPREFERENCES
.area _ZPCODE
.area _ZPDATA

f_word_id = 0
f_fail_id = 1
f_native_error_id = 2
f_open_path_id = 3
f_open_file_id = 4
f_stat_file_id = 5
f_remove_file_id = 6
f_close_file_id = 7
f_within_id = 8
f_heap_span_id = 9
f_readable_id = 10
f_descriptor_id = 11
f_initialize_id = 12
f_bound_id = 13
f_flags_id = 14
f_device_access_id = 15
f_standards_id = 16
f_standard_identity_id = 17
f_bookkeeping_status_id = 18
f_write_bytes_id = 19
f_transfer_id = 20
f_seek_file_id = 21
f_header_id = 22
f_access_id = 23
f_text_position_id = 24
f_text_remember_id = 25
f_text_bytes_id = 26
f_disk_open_id = 27
f_disk_release_id = 28
f_serial_open_id = 29
f_text_peek_id = 30
f_text_take_id = 31
f_text_space_id = 32
f_text_predicate_id = 33
f_text_input_target_id = 34
f_text_output_value_id = 35
f_text_read_id = 36
f_text_readln_id = 37
f_text_write_id = 38
f_flush_id = 39
f_close_record_id = 40
f_activate_id = 41
f_component_id = 42
f_blocks_id = 43
f_logical_device_id = 44
f_assigned_path_id = 45
f_rename_close_id = 46
f_rename_read_id = 47
f_rename_end_id = 48
f_rename_file_id = 49
f_serial_stream_id = 50
f_logical_stream_id = 51
f_zf_op_reset_id = 52
f_zf_op_rewrite_id = 53
f_zf_op_close_id = 54
f_zf_op_read_id = 55
f_zf_op_write_id = 56
f_zf_op_position_id = 57
f_zf_op_seek_id = 58
f_zf_op_flush_id = 59
f_zf_op_erase_id = 60
f_zf_op_rename_id = 61
f_zf_op_blocks_id = 62
f_zf_op_text_read_id = 63
f_zf_op_text_readln_id = 64
f_zf_op_predicate_id = 65
f_zf_op_text_write_id = 66
f_zf_op_begin_id = 67
f_zf_op_path_id = 68
f_dispatch_id = 69
f_zf_dispatch_id = 70
f_zf_cleanup_id = 71
f_resolve_id = 72
f_path_resolve_id = 73
f_path_directory_id = 74
f_zf_entry_id = 75
f_zf_operations_address_id = 76
f_read_id = 77
f_write_id = 78
f_open_id = 79
f_close_id = 80
f_stat_id = 81
f_seek_id = 82
f_ioctl_id = 83
f_curdir_id = 84
f_rm_id = 85
f_strlen_id = 86
f_strcat_id = 87
f_memchr_id = 88
f_sdcc_enter_ix_id = 89
f_muluint2ulong_id = 90
f_strcmp_id = 91
f_memcmp_id = 92
f_sdcc_call_hl_id = 93
sf_data_id = 94

sf_next_fragment = 0
.macro fragment name
    .ifne name'_id-sf_next_fragment
        .error 1 ; Fragment IDs must follow the directory order.
    .endif
    sf_next_fragment = sf_next_fragment+1
    .area _ZPFRAGMENTS
    .dw name-sf_code, name'_end-name
    .dw (name'_refs-sf_refs)/8, (name'_refs_end-name'_refs)/8
    .area _ZPFLAGS
    .db 0
    .area _ZPREFERENCES
name'_refs:
    .area _ZPCODE
name:
.endm
.macro endfragment name
name'_end:
    .area _ZPREFERENCES
name'_refs_end:
    .area _ZPCODE
.endm
; Record an address operand for the Pascal static linker. The assembler owns
; opcode encoding and every offset; no host object reader or linker is used.
.macro reference site, owner, displacement, destination, target, kind
site:
    .area _ZPREFERENCES
    .dw site-owner+displacement, 1, destination'_id+(kind*16384), target-destination
    .area _ZPCODE
.endm
.macro export name, destination, target, length
    .db length,0
    .dw 0,65535,0,destination'_id,target-destination,0
    .ascii /name/
.endm
.area _ZPHEADER
    .ascii /ZPU1/
    .dw 1,zpu_end
    .dw 0,0
    .db 0x7f,0x8f,0x0c,0x25
    .dw 1
    .db 7,18
    .dw 19,0,0,sf_blob
    .dw 0,0
    .dw sf_code_end-sf_code,sf_data_end-sf_data
    .dw 0,0
    .dw sf_exports,sf_exports,sf_exports,0
.area _ZPEXPORTS
    .ascii "sysfileT:/lib/SYSFILE.PAS"
sf_exports:

    export dispatch, f_zf_entry, _zf_entry, 8
    export table, f_zf_operations_address, _zf_operations_address, 5
    export reset, f_zf_op_reset, _zf_op_reset, 5
    export rewrite, f_zf_op_rewrite, _zf_op_rewrite, 7
    export close, f_zf_op_close, _zf_op_close, 5
    export read, f_zf_op_read, _zf_op_read, 4
    export write, f_zf_op_write, _zf_op_write, 5
    export position, f_zf_op_position, _zf_op_position, 8
    export seek, f_zf_op_seek, _zf_op_seek, 4
    export flush, f_zf_op_flush, _zf_op_flush, 5
    export erase, f_zf_op_erase, _zf_op_erase, 5
    export rename, f_zf_op_rename, _zf_op_rename, 6
    export blocks, f_zf_op_blocks, _zf_op_blocks, 6
    export text_read, f_zf_op_text_read, _zf_op_text_read, 9
    export text_readln, f_zf_op_text_readln, _zf_op_text_readln, 11
    export predicate, f_zf_op_predicate, _zf_op_predicate, 9
    export text_write, f_zf_op_text_write, _zf_op_text_write, 10
    export begin, f_zf_op_begin, _zf_op_begin, 5
    export path, f_zf_op_path, _zf_op_path, 4
.area _ZPBLOB
sf_blob:
    .ascii /ZPF1/
    .dw 1,sf_data_id+1,(sf_refs_end-sf_refs)/8,sf_code_end-sf_code+sf_data_end-sf_data,0,0
.area _ZPREFERENCES
sf_refs:
.area _ZPCODE
sf_code:
.include "support.asm"
.include "validation.asm"
.include "transport.asm"
.include "text.asm"
.include "files.asm"
.include "devices.asm"
.include "dispatch.asm"
.include "paths.asm"
.include "gate.asm"
.include "syscalls.asm"
.include "helpers.asm"
sf_code_end:
.ifne sf_data_id-sf_next_fragment
    .error 1 ; Workspace is the last fragment.
.endif
.area _ZPFLAGS
    .db 0
.area _ZPFRAGMENTS
    .dw sf_code_end-sf_code,sf_data_end-sf_data,(sf_refs_end-sf_refs)/8,0
.area _ZPREFERENCES
sf_refs_end:
.include "workspace.asm"
