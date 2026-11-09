; Test-only SER0 transport. Installed in the benchmark's own COM after all
; real disk operations; no kernel/emulator code is changed. The file engine
; and its Pascal state/validation/serial-mode handling remain production code.
.module sysfile_serial_mock
.area _MOCK
.globl _mock_open,_mock_close,_mock_read,_mock_write,_mock_ioctl
INPUT = 0xf100
INPUT_POS = 0xf110
OUTPUT_POS = 0xf111
ATTRIBUTE = 0xf112
OPENS = 0xf114
CLOSES = 0xf115
IOCTLS = 0xf116
WRITE_ERROR = 0xf117
IOCTL_FAIL_AT = 0xf118
READ_ERROR = 0xf119
OPEN_ERROR = 0xf11a
OUTPUT = 0xf200
_mock_open:
    pop hl
    inc sp
    push hl
    ld hl,#OPENS
    inc (hl)
    ld a,(#OPEN_ERROR)
    or a
    jr z,open_ok
    neg
    ret
open_ok:
    ld a,#127
    ret
_mock_close:
    ld hl,#CLOSES
    inc (hl)
    xor a
    ret
_mock_read:
    pop hl
    ex (sp),hl
    push hl
    ld a,(#READ_ERROR)
    or a
    jr nz,read_error
    ld a,(#INPUT_POS)
    cp #2
    jr nc,read_empty
    ld hl,#INPUT
    ld l,a
    ld a,(hl)
    ld (de),a
    ld a,(#INPUT_POS)
    inc a
    ld (#INPUT_POS),a
    pop hl
    ld (hl),#1
    inc hl
    ld (hl),#0
    xor a
    ret
read_empty:
    pop hl
    ld (hl),#0
    inc hl
    ld (hl),#0
    xor a
    ret
read_error:
    pop hl
    ret
_mock_write:
    pop hl
    ex (sp),hl
    ld a,(#WRITE_ERROR)
    or a
    ret nz
    ld c,(hl)
    inc hl
    ld b,(hl)
    push bc
    ld a,(#OUTPUT_POS)
    ld hl,#OUTPUT
    ld l,a
    ex de,hl
    ldir
    pop bc
    ld a,(#OUTPUT_POS)
    add a,c
    ld (#OUTPUT_POS),a
    xor a
    ret
_mock_ioctl:
    ld c,l
    pop hl
    ex (sp),hl
    ld a,(#IOCTLS)
    inc a
    ld (#IOCTLS),a
    ld e,a
    ld a,(#IOCTL_FAIL_AT)
    cp e
    jr z,ioctl_error
    ld a,c
    cp #0x80
    jr z,ioctl_get
    ld a,l
    ld (#ATTRIBUTE),a
    xor a
    ret
ioctl_get:
    ld a,(#ATTRIBUTE)
    ld (hl),a
    xor a
    ret
ioctl_error:
    ld a,#1
    ret
