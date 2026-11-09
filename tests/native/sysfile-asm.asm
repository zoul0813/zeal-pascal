; Exercise exactly the production ASM routines with the native file-core suite.
; The normal assembler/linker resolves addresses in this test image; ZPU
; relocation records are independently exercised by Pascal integration tests.
.module sysfile_test
.globl _zf_dispatch,_zf_cleanup,_zf_operations
.globl _zf_op_reset,_zf_op_rewrite,_zf_op_close,_zf_op_read,_zf_op_write
.globl _zf_op_seek,_zf_op_position,_zf_op_flush,_zf_op_erase,_zf_op_blocks
.globl _zf_op_rename,_zf_op_text_read,_zf_op_text_readln,_zf_op_predicate
.globl _zf_op_text_write,_zf_op_begin,_zf_op_path,_path_resolve,_path_directory
.globl _read,_write,_open,_close,_stat,_seek,_rm,_ioctl,_curdir
.globl _strlen,_strcat,_memchr,_memcmp,_strcmp,___muluint2ulong
.globl ___sdcc_enter_ix,___sdcc_call_hl
.macro fragment name
    .area _TEXT
name:
.endm
.macro endfragment name
name'_end:
.endm
.macro reference site, owner, displacement, destination, target, kind
site:
.endm
.area _TEXT
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
; Workspace uses the same offsets and sizes as the production assembly.
.include "workspace-test.asm"
; Test fixtures need directory syscalls; production SYSFILE does not.
.area _TEXT
.globl _mkdir,_chdir
_mkdir:
    ex de,hl
    ld l,#8
    rst 8
    ret
_chdir:
    ex de,hl
    ld l,#9
    rst 8
    ret
