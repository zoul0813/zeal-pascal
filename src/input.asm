.module input
.globl read_line,read_string,input_begin,input_value,input_end
.globl memory_mode,call_error,val_dispatch,buflen,cbreak,user_break
.globl _key_event,c_save,c_restore,io_check,io_fail,io_status
.globl line_history
.globl input_ready,input_advance,input_source,line_next,line_remaining,line_eof,line_valid
.globl input_key,input_raw_key,input_mode,input_cleanup,input_fail,input_error,echo_fd,input_echo
.area _REAL_DATA
line_next: .ds 2
line_remaining: .ds 1
line_valid: .ds 1               ; bit0 primary current byte; bit1 pending B+ line
line_eof: .ds 1
primary_byte: .ds 1
line_buffer: .ds 129             ; 127 content bytes plus ordinary CR/LF
.equ line_history, 0xff80       ; private transition data, never stack payload
history_length: .ds 1
input_length: .ds 1
input_max: .ds 1
echo_fd: .ds 1
raw_bytes: .ds 2
.equ echo_byte, raw_bytes       ; read bytes have been decoded before synchronous echo
raw_pending: .ds 1
raw_second: .ds 1
input_token: .ds 31             ; short string: at most30 numeric bytes
.area _CODE
; Bit0 forces a fresh B+ line. Bit1 primes bare Read immediately; fields stay
; lazy until their destination callbacks finish. Primary Input/Output share a
; current-byte cache; the physical line cursor belongs to input_source instead.
input_begin::
    push af
    and #1
    jr z,input_initialized
    ld a,#2
    ld (line_valid),a
    xor a
    ld (line_remaining),a
input_initialized:
    pop af
    bit 1,a
    ret z
    jp input_ready
input_ready:
    ld a,(io_status)
    or a
    jp nz,io_check
    ld hl,#line_valid
    bit 0,(hl)
    jr nz,input_current
    push bc
    push de
    call input_source
    ld (primary_byte),a
    pop de
    pop bc
    ld hl,#line_valid
    set 0,(hl)
input_current:
    ld a,(primary_byte)
    ret
; The physical buffered source is shared, but each Text binding owns its own
; current-byte cache. Fetch advances this cursor exactly once, including CR/LF.
input_source:
    ld a,(line_remaining)
    or a
    jr nz,input_source_byte
    ld a,(line_eof)
    or a
    ld a,#26
    ret nz
    call input_line
input_source_byte:
    ld hl,(line_next)
    ld a,(hl)
    inc hl
    ld (line_next),hl
    push af
    ld a,(line_remaining)
    dec a
    ld (line_remaining),a
    pop af
    ret
 ; The runtime owns the selected limit and editing state. Excess bytes are
; ignored until a terminator, but deletion can make room for replacement text.
read_line::
    ; Keep the original native helper's fresh-line contract. Its current byte
    ; is available to READSTRING or the ordinary B- path through the same cache.
    ld a,#3
    jp input_begin
input_line:
    ld a,#255
    ld (echo_fd),a
    ld a,(buflen)
    cp #128
    jr c,input_limit_ready
    ld a,#0x91
    ld b,#2
    jp call_error
input_limit_ready:
    ld (input_max),a
    xor a
    ld (input_length),a
    ld de,#0                    ; blocking raw while editing
    call input_mode
    or a
    jp nz,input_fail
    call input_open
input_edit:
    call input_key
    cp #10
    jp z,input_finished
    cp #13
    jp z,input_finished
    cp #26
    jp z,input_eof
    cp #3
    jr z,input_control_c
    cp #8
    jr z,input_backspace
    cp #127
    jr z,input_backspace
    cp #0x9c                    ; native Delete key
    jr z,input_backspace
    cp #27
    jr z,input_clear
    cp #0xf0                    ; native Escape key
    jr z,input_clear
    cp #24
    jr z,input_clear
    cp #4
    jr z,input_one_recall
    cp #18
    jr z,input_all_recall
    cp #32
    jr c,input_edit             ; other controls (including Tab) are ignored
    cp #127
    jr nc,input_edit
    call input_append
    jr input_edit
input_backspace:
    call input_delete
    jr input_edit
input_clear:
    ld a,(input_length)
    or a
    jr z,input_edit
    call input_delete
    jr input_clear
input_one_recall:
    call input_recall
    jr input_edit
input_all_recall:
    ld a,(input_length)
    push af
    call input_recall
    pop bc
    ld a,(input_length)
    cp b
    jr nz,input_all_recall
    jr input_edit
input_control_c:
    ld a,(cbreak)
    or a
    jr z,input_edit
    ld hl,#input_break_text
input_break_message:
    ld a,(hl)
    or a
    jr z,input_break_cleanup
    push hl
    call input_echo
    pop hl
    inc hl
    jr input_break_message
input_break_cleanup:
    call input_cleanup
    or a
    jp nz,input_error
    jp user_break
input_eof:
    ld a,#1
    ld (line_eof),a
input_finished:
    ld hl,#line_buffer
    ld (line_next),hl
    ld a,(input_length)
    ld (history_length),a
    ld c,a
    ld b,#0
    ld de,#line_history
    or a
    jr z,input_history_ready
    ldir
input_history_ready:
    ld a,#127
    ld (buflen),a
    ld (hl),#26
    ld a,(line_eof)
    or a
    jr nz,input_marker_ready
    ld a,(line_valid)
    bit 1,a
    jr nz,input_marker_ready
    ld (hl),#13
    inc hl
    ld (hl),#10
    ld a,(input_length)
    add a,#2
    ld (line_remaining),a
    ld a,#13
    call input_echo
    ld a,#10
    call input_echo
    jr input_line_ready
input_marker_ready:
    ld a,(input_length)
    inc a
    ld (line_remaining),a
input_line_ready:
    ; Physical refill never invalidates a different binding's current byte.
    ; Publish the complete extent before echo/cleanup can fail nonlocally.
    ld a,(line_valid)
    and #1
    ld (line_valid),a
    call input_cleanup
    or a
    jp nz,input_error
    ret
input_append:
    push af
    ld a,(input_length)
    ld b,a
    ld a,(input_max)
    cp b
    jr z,input_full
    jr c,input_full
    ld e,b
    ld d,#0
    ld hl,#line_buffer
    add hl,de
    pop af
    ld (hl),a
    push af
    ld a,b
    inc a
    ld (input_length),a
    pop af
    jp input_echo
input_full:
    pop af
    ret
input_delete:
    ld a,(input_length)
    or a
    ret z
    dec a
    ld (input_length),a
    ld a,#8
    call input_echo
    ld a,#32
    call input_echo
    ld a,#8
    jp input_echo
input_recall:
    ld a,(history_length)
    ld b,a
    ld a,(input_length)
    cp b
    ret nc
    ld e,a
    ld d,#0
    ld hl,#line_history
    add hl,de
    ld a,(hl)
    jp input_append
; Decode only presses, retaining the second byte of each raw release pair.
; The shared C decoder owns a separate modifier state in this runtime image.
; The compiler can bind this private dispatch operand to its generated
; typeahead owner. The default has the same raw decoder and no extra state.
input_key:
    jp input_raw_key
input_raw_key:
    ld a,(raw_pending)
    or a
    jr z,input_key_read
    xor a
    ld (raw_pending),a
    ld a,(raw_second)
    jr input_key_decode
input_key_read:
    ld de,#raw_bytes
    ld bc,#2
    ld hl,#0x0100
    call input_syscall
    or a
    jp nz,input_fail
    ld a,b
    or a
    jp nz,input_fail
    ld a,c
    or a
    ret z                      ; nonblocking polls may find no physical event
    cp #3
    jp nc,input_fail
    cp #2
    jr nz,input_key_first
    ld a,(raw_bytes+1)
    ld (raw_second),a
    ld a,#1
    ld (raw_pending),a
input_key_first:
    ld a,(raw_bytes)
input_key_decode:
    call c_save
    call _key_event
    call c_restore
    or a
    jr z,input_key
    ret
; Echo is a separate supported console handle, never Pascal Output/stdout.
input_open:
    ; read_line/cleanup own the invalid255 state before every open attempt.
    ld bc,#input_video_name
    ld hl,#0x0102
    call input_syscall
    or a
    jp m,input_fail
    ld (echo_fd),a
    ret
input_echo:
    ld (echo_byte),a
    ld de,#echo_byte
    ld bc,#1
    ld a,(echo_fd)
    ld h,a
    ld l,#1
    call input_syscall
    or a
    jp nz,input_fail
    ld a,b
    or a
    jp nz,input_fail
    ld a,c
    cp #1
    jp nz,input_fail
    ret
input_close:
    ld a,(echo_fd)
    bit 7,a
    jr z,input_close_owned
    xor a
    ret
input_close_owned:
    ld h,a
    ld a,#255
    ld (echo_fd),a
    ld l,#3
    call input_syscall
    ; VID0 has no close operation (ERR_NOT_IMPLEMENTED=2), but VFS always
    ; releases its descriptor after the driver returns. Only that known
    ; result is harmless; other failures remain visible to the caller.
    cp #2
    ret nz
    xor a
    ret
input_mode:
    ld hl,#0x0107
    ld c,#0
    jp input_syscall
input_cleanup:
    ld de,#0
    ld a,(memory_mode)
    or a
    jr nz,input_restore_mode
    ld e,#4
input_restore_mode:
    call input_mode
    push af
    call input_close
    ld b,a
    pop af
    or b
    ret
input_fail:
    push af                    ; retain the primary native error across cleanup
    call input_cleanup
    pop af
input_error:
    ; IOCALL cleared the detail's high byte before this first input failure.
    ld (0xfe7a),a
    ld a,#0x20
    jp io_fail
input_video_name: .asciz "#VID0"
input_break_text: .asciz "\r\nUser break\r\n"
; Kernel calls may replace page3; buffers live in the fixed runtime prefix.
; Preserve this invocation's source PC and return status/count across each call.
input_syscall:
    push hl
    ld hl,(0x40be)
    ex (sp),hl
    rst 8
    pop hl
    ld (0x40be),hl
    ret
read_string::
    push de
    inc de
    ld b,#0
read_string_byte:
    ld a,c
    or a
    jr z,read_string_done
    call input_ready
    cp #13
    jr z,read_string_done
    cp #26
    jr z,read_string_done
    ld (de),a
    inc de
    inc b
    dec c
    call input_advance
    jr read_string_byte
read_string_done:
    pop hl
    ld (hl),b
    ret
input_end::
    ld a,(io_status)
    or a
    jp nz,io_check
    ; A Char may have consumed the fresh line's Ctrl-Z already. ReadLn still
    ; ends that line; it must not fetch another one merely to discard it.
    ld a,(line_valid)
    bit 1,a
    jr nz,input_end_read
    ld a,(line_remaining)
    or a
    jr nz,input_end_read
    ld a,(primary_byte)
    cp #26
    jr z,input_end_marker
input_end_read:
    call input_ready
    cp #26
    jr z,input_end_marker
    call input_advance
    cp #10
    ret z
    jr input_end
input_end_marker:
    call input_advance
    xor a
    ld (primary_byte),a
    ld a,(line_eof)
    or a
    ret nz
    call input_open
    ld a,#13
    call input_echo
    ld a,#10
    call input_echo
    call input_close
    or a
    jp nz,input_fail
    ret
; The selected destination and kind are invocation-owned across device/parser
; calls. Integers store either one or two bytes; input does not add R+ checks.
input_value::
    push hl
    push bc
    call input_ready
    pop bc
    pop de
    ld a,b
    cp #1
    jp z,read_string
    cp #3
    jr z,input_character
    push bc
    push de
    call input_number
    pop bc                      ; destination
    push af                     ; assignment and numeric kind carry
    ld a,d
    or e
    jr nz,input_invalid
    pop af
    pop de                      ; D = destination kind
    inc a
    dec a
    ret z
    jr c,input_integer
    ld d,b
    ld e,c
    ld bc,#6
    ldir
    ret
input_integer:
    ld a,l
    ld (bc),a
    ld a,d
    cp #4
    ret z
    inc bc
    ld a,h
    ld (bc),a
    ret
input_invalid:
    pop af
    pop de
    ld a,#0x10
    jp io_fail
input_character:
    ld a,(primary_byte)
    ld (de),a
input_advance:
    ld hl,#line_valid
    res 0,(hl)
    ret
input_number:
    ld a,b
    cp #5
    ld a,#15                    ; shared Val Integer grammar
    jr nz,input_mode_ready
    inc a                       ; Val Real
input_mode_ready:
    push af
input_skip:
    call input_ready
    cp #26
    jr z,input_token_begin
    cp #33
    jr nc,input_token_begin
    call input_advance
    jr input_skip
input_token_begin:
    ld de,#input_token+1
    ld c,#0
input_token_byte:
    call input_ready
    cp #26
    jr z,input_token_ready
    cp #33
    jr c,input_token_ready
    ld (de),a
    inc de
    inc c
    call input_advance
    ld a,c
    cp #30
    jr c,input_token_byte
input_token_ready:
    ld a,c
    ld (input_token),a
    ld hl,#input_token
    pop af
    jp val_dispatch
