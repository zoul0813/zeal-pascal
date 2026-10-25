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
.globl _console_scratch
.globl _platform_supported

; Project-owned startup. Bank payloads temporarily occupy 8000..EFFF in the
; loaded executable, including future C data. Capture them BEFORE C startup.
; The application takes no command-line arguments. SDK syscall glue is reused.
.area _HEADER
    call code_load
    call init_globals
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
code_load:
    xor a
    ld (_code_owned),a
    ld (_boot_error),a
    call _platform_supported
    or a
    jr nz,allocate
    ld a,#3                     ; ERR_NOT_SUPPORTED, before any palloc/MMU write
    ld (_boot_error),a
    ret
allocate:
    ld a,(_code_owned)
    ld e,a
    ld d,#0
    ld hl,#_code_pages
    add hl,de
    call _palloc
    or a
    jr nz,allocation_failed
    ld hl,#_code_owned
    inc (hl)
    ld a,(hl)
    cp #3
    jr nz,allocate
    ld hl,#0x8000
    ld a,(_code_pages)
    ld bc,(_code_sizes)
    call copy_bank
    ld a,(_code_pages+1)
    ld bc,(_code_sizes+2)
    call copy_bank
    ld a,(_code_pages+2)
    ld bc,(_code_sizes+4)
    jp copy_bank
allocation_failed:
    ld (_boot_error),a
    jp _code_release

; Stage each chunk in retained page 1 before mapping its destination in page
; 2. Restore the original page 2 before fetching more payload. Keep the kernel
; and stack mapped, and leave interrupts untouched: a whole-bank DI/LDIR loses
; PS/2 input arriving during launch (including the shell Enter release).
copy_bank:
    ld (_copy_destination),a
    ld a,#0x80
    in a,(0xf2)
    ld (_copy_original),a
    ld de,#0x8000
copy_next:
    ld a,b
    or c
    ret z
    push bc                     ; remaining
    push de                     ; destination address
    ld a,b
    or a
    jr nz,full_chunk
    ld a,c
    cp #128
    jr c,chunk_ready
full_chunk:
    ld bc,#128
chunk_ready:
    push bc
    ld de,#_console_scratch
    ldir                        ; source advances in its original mapping
    pop bc
    pop de
    push hl                     ; next source address
    push bc                     ; chunk length
    ld hl,#_console_scratch
    ld a,(_copy_destination)
    out (0xf2),a
    ldir
    ld a,(_copy_original)
    out (0xf2),a
    pop bc
    pop hl
    ex (sp),hl                  ; HL=remaining, stack=next source
    or a
    sbc hl,bc
    ld b,h
    ld c,l
    pop hl
    jr copy_next

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
    or a
    ret z
    dec a
    ld e,a
    ld d,#0
    ld hl,#_code_pages
    add hl,de
    ld a,(hl)
    call _pfree
    or a
    jr nz,release_failed
    ld hl,#_code_owned
    dec (hl)
    jr _code_release
release_failed:
    ld (_boot_error),a           ; retain ownership if pfree did not succeed
    ret
_code_sizes:: .ds 6            ; packager fills lengths of the three payloads
_code_pages:: .ds 3
_code_owned:: .ds 1
_boot_error:: .ds 1
_copy_original: .ds 1
_copy_destination: .ds 1
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
