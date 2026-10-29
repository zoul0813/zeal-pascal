.module transfer_native
.globl _transfer_file_call,_transfer_commit,_transfer_install
.globl transfer_commit_start,transfer_commit_end
.area _TEXT
; A=file operation mode, DE=stack job. Save the enclosing transport's prefix,
; mode and borrowed handle across a nested FileGate call. Mode1 never frees.
_transfer_file_call::
    push af
    ld a,(0xfe74)
    push af
    ld a,(0xfe76)
    push af
    ld a,(0xfe7e)
    push af
    ld a,(0xfe77)
    ld (0xfe74),a
    ld a,(0xfe75)
    ld (0xfe76),a
    ld hl,#7
    add hl,sp
    ld a,(hl)                 ; saved A, above the saved flags byte
    ex de,hl
    call 0xfe90
    pop af
    ld (0xfe7e),a
    pop af
    ld (0xfe76),a
    pop af
    ld (0xfe74),a
    pop af
    ret

_transfer_commit::
    ld sp,#0xfc00             ; abandon every old frame before replacing code
    call _transfer_install
    ld hl,#commit_start
    ld de,#0xfc00
    ld bc,#commit_end-commit_start
    ldir
    jp 0xfc00
; Position independent until the prefix is restored. The nine address/value
; pairs atFC30 are inactive normal-program context, never RAM worker metadata.
transfer_commit_start::
commit_start:
    ld a,(0xfe76)
    out (0xf1),a
    ld hl,#0xfc30
    ld b,#9
commit_word:
    ld e,(hl)
    inc hl
    ld d,(hl)
    inc hl
    ld a,(hl)
    inc hl
    ld (de),a
    inc de
    ld a,(hl)
    inc hl
    ld (de),a
    djnz commit_word
    ld hl,(0x401e)
    inc hl
    ld sp,hl
    ld ix,#0
    ld hl,(0x4014)
    jp (hl)
commit_end:
transfer_commit_end::
