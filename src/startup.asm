; SPDX-FileCopyrightText: 2023 Zeal 8-bit Computer <contact@zeal8bit.com>
; SPDX-License-Identifier: Apache-2.0
; The C global-initialization sequence follows the installed SDK's CRT.
; Bank loading, transitions and ownership are project implementation code.
.module pascal_startup
.globl _main, _fflush_stdout, _exit, _palloc, _pfree
.globl s__DATA, l__DATA, s__BSS, l__BSS
.globl s__INITIALIZER, l__INITIALIZER, s__INITIALIZED
.globl _code_sizes, _code_pages, _code_owned, _boot_error
.globl _code_select, _code_restore, _code_release
.globl _code_required, _code_digest
.globl _code_phase_fd, _code_phase_cleanup, _close
.globl get_bank, set_bank

; SDK-compatible command-tail entry; application code loads the library after
; C data initialization. No payload overlaps the C data or startup stack.
.area _HEADER
    push de
    push bc
    call init_globals
    pop hl
    pop de
    ld a,h
    or l
    jr z,no_arguments
    push de
    ld hl,#0
    add hl,sp
    ld de,#1
    ex de,hl
no_arguments:
    call _main
    push de
    call _code_release
    call _fflush_stdout
    pop de
    ld a,e
    jp _exit
.area _GSINIT
clear:
    ld a,b
    or c
    ret z
    ld (hl),#0
    dec bc
    ld a,b
    or c
    ret z
    ld d,h
    ld e,l
    inc de
    ldir
    ret
init_globals:
    ld bc,#l__DATA
    ld hl,#s__DATA
    call clear
    ld bc,#l__BSS
    ld hl,#s__BSS
    call clear
    ld bc,#l__INITIALIZER
    ld a,b
    or c
    jr z,initialized
    ld hl,#s__INITIALIZER
    ld de,#s__INITIALIZED
    ldir
initialized:
.area _GSFINAL
    ret
.area _INITIALIZER
.area _HOME
; Exact physical mapping is the ownership token; no parallel current-bank
; variable can become stale across RAM execution or nested resident services.
_code_select::                 ; A=0 editor, 1 compiler, 2 file services
    ld e,a
    ld d,#0
    ld hl,#_code_pages
    add hl,de
    ld e,(hl)
    ld a,#0x80
    in a,(0xf2)
    ld d,a
    ld a,e
    out (0xf2),a
    ld a,d
    ret
_code_restore::                ; A=previous physical page
    out (0xf2),a
    ret
_code_release::
    ld a,(_code_owned)
    cp #14
    jr c,release_loop
    ld a,(_code_phase_fd)
    bit 7,a
    jr nz,release_loop          ; a failed initial load never executes its banks
    call _code_phase_cleanup
    or a
    ret z                      ; failed pending-page release keeps every owner
release_loop:
    ld a,(_code_owned)
    or a
    jr z,release_library
    dec a
    ld e,a
    ld d,#0
    ld hl,#_code_pages
    add hl,de
    ld a,(hl)
    cp #255
    jr z,release_next
    call _pfree
    or a
    jr nz,release_failed
release_next:
    ld hl,#_code_owned
    dec (hl)
    jr release_loop
release_failed:
    ld (_boot_error),a           ; retain ownership if pfree did not succeed
    ret
release_library:
    ld a,(_code_phase_fd)
    bit 7,a
    ret nz
    push af
    ld a,#255
    ld (_code_phase_fd),a
    pop af
    call _close
    or a
    ret z
    ld (_boot_error),a
    ret
; The SDK's standard banked-call trampoline calls these callbacks. Logical
; bank IDs are 1..16; saved tokens are actual RAM pages 32..63. Both callbacks
; preserve BC/DE/HL. There is no software current-bank state to go stale.
get_bank::
    ld a,#0x80
    in a,(0xf2)
    ret
set_bank::
    cp #32
    jr nc,map_bank
    push hl
    push bc
    dec a
    ld c,a
    ld b,#0
    ld hl,#_code_pages
    add hl,bc
    ld a,(hl)
    pop bc
    pop hl
map_bank:
    out (0xf2),a
    ret
_code_sizes:: .ds 32
_code_pages:: .ds 16
_code_owned:: .ds 1
_boot_error:: .ds 1
_code_required:: .ds 1          ; packager fills count and exact library SHA-256
_code_digest:: .ds 32
.area _SYSTEM
.area _TEXT
.area _RODATA
.area _INITIALIZED
.area _BSEG
.area _BSS
.area _DATA
.area _HEAP
.area _BANK1
.area _BANK2
.area _BANK3
.area _BANK4
.area _BANK5

.area _BANK6

.area _BANK7

.area _BANK8
