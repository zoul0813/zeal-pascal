.module transfer_driver
.globl _driver_gate,_driver_gate_bytes,_driver_profile,_driver_private
.globl transfer_driver_end
.area _HEADER (ABS)
.org 0x8800
; Test-only bootstrap in the saved image's unused globals gap. Production
; compiler acquisition/path handling is a subsequent integration, not implied.
    ld a,#0x40
    in a,(0xf1)
    ld (0xfe76),a
    ld hl,#_driver_gate
    ld de,#0xfc80
    ld bc,#_driver_gate_bytes
    ldir
    ld hl,#0xf600
    ld de,#0xf601
    ld bc,#53
    ld (hl),#0
    ldir
    ; With no coordinator, dispatch must report20 and leave any file owner.
    ld hl,#0xf600
    ld a,#3
    call 0xfc80
    ld hl,(0xf632)
    ld de,#0x20
    or a
    sbc hl,de
    jp nz,failed
    ; Each program may release only its own palloc pages. The parent lends
    ; immutable bytes; this child acquires its own coordinator before copying.
    ld l,#25
    rst 8
    or a
    jp nz,failed
    ld a,b
    ld (0xfe75),a
    ld a,#0x80
    in a,(0xf2)
    ld (0xfc7f),a
    ld hl,#copy_module
    ld de,#0xfc00
    ld bc,#copy_module_end-copy_module
    ldir
    jp 0xfc00
module_copied:
    ld hl,#1024
    ld (0xfe7c),hl
    ld hl,(0x4014)
    ld (0xf602),hl
    ld hl,(0x4016)
    ld (0xf604),hl
    ld hl,(0x401a)
    ld (0xf606),hl
    ld hl,(0x401e)
    ld (0xf608),hl
    ld hl,(0x415c)
    ld (0xf60a),hl
    ld hl,(0x412c)
    ld (0xf60c),hl
    ld hl,#0xf600
    ld (0xf60e),hl
    ld hl,#_driver_profile
    ld de,#0xf610
    ld bc,#32
    ldir
    ld hl,#0xf680
    ld (0xf630),hl
    ; A direct-Memory attempt must reject before staging/owner handoff.
    ld a,#1
    ld (0xfe78),a
    ld hl,#0xf600
    ld a,#3
    call 0xfc80
    ld hl,(0xf632)
    ld de,#21
    or a
    sbc hl,de
    jp nz,failed
    xor a
    ld (0xfe78),a
    ld hl,#missing
    ld de,#0xf680
    ld bc,#missing_end-missing
    ldir
    ld hl,#0xf600
    ld a,#3
    call 0xfc80
    ld hl,(0xf632)
    dec hl
    ld a,h
    or l
    jp nz,failed
    ld hl,#replacement
    ld de,#0xf680
    ld bc,#replacement_end-replacement
    ldir
    ld hl,#0xf600
    ld a,#3
    call 0xfc80
    ld hl,(0xf632)
    ld a,h
    or l
    jp nz,failed
    ; Native input cleanup executes with the real prefix mapped, before the
    ; coordinator retires ending owners and abandons every old Pascal frame.
    ld hl,(_driver_private+18)
    call input_cleanup
    or a
    jp nz,failed
    ld hl,#1
    ld (0xf600),hl
    ld hl,#0xf600
    ld a,#3
    call 0xfc80
failed:
    ld hl,#7
    jp 0x40a9
input_cleanup:
    jp (hl)
copy_module:
    ld a,(0xfe75)
    out (0xf2),a
    ld a,(0xff01)
    out (0xf1),a
    ld hl,#0x4000
    ld de,#0x8000
    ld bc,#0x4000
    ldir
    ld a,(0xfe76)
    out (0xf1),a
    ld a,(0xfc7f)
    out (0xf2),a
    jp module_copied
copy_module_end:
missing: .asciz "H:/MISSING.CHN"
missing_end:
replacement: .asciz "H:/CHAIN.CHN"
replacement_end:
transfer_driver_end::
