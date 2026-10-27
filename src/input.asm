.module input
.globl read_line,read_string,input_begin,input_value,input_end
.globl memory_mode,io_error,call_error,val_dispatch,buflen,cbreak,user_break
.globl _key_event,c_save,c_restore,io_check,io_fail,io_status
.globl line_history
.area _REAL_DATA
line_next: .ds 2
line_remaining: .ds 1
line_valid: .ds 1               ; 0 unloaded, 1 buffered, 2 end marker consumed
line_eof: .ds 1
line_buffer: .ds 128
.equ line_history, 0xff80       ; private transition data, never stack payload
history_length: .ds 1
input_length: .ds 1
input_max: .ds 1
echo_fd: .ds 1
echo_byte: .ds 1
raw_bytes: .ds 2
raw_pending: .ds 1
raw_second: .ds 1
input_token: .ds 31             ; short string: at most30 numeric bytes
.area _CODE
; Bit0 selects fresh buffering. Bit1 primes bare Read immediately; calls with
; destinations stay lazy until after their callbacks. Even B- starts another
; line when the preceding call exhausted its buffer.
input_begin::
    push af
    and #1
    jr nz,input_new
    ld a,(line_remaining)
    or a
    jr nz,input_initialized
input_new:
    xor a
    ld (line_valid),a
input_initialized:
    pop af
    bit 1,a
    ret z
    jp input_ready
input_ready:
    ld a,(io_status)
    or a
    jp nz,io_check
    ld a,(line_valid)
    cp #1
    ret z
    ld a,(line_eof)
    or a
    jr z,read_line
    xor a
    ld (line_remaining),a
    ld a,#1
    ld (line_valid),a
    ret
 ; The runtime owns the selected limit and editing state. Excess bytes are
; ignored until a terminator, but deletion can make room for replacement text.
read_line::
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
    jp nz,io_error
    jp user_break
input_eof:
    ld a,#1
    ld (line_eof),a
input_finished:
    ld hl,#line_buffer
    ld (line_next),hl
    ld a,(input_length)
    ld (line_remaining),a
    ld (history_length),a
    ld c,a
    ld b,#0
    ld de,#line_history
    or a
    jr z,input_history_ready
    ldir
input_history_ready:
    ld (hl),#26                 ; spare byte, including at the127-byte limit
    ld a,#1
    ld (line_valid),a
    ld a,#127
    ld (buflen),a
    call input_cleanup
    or a
    jp nz,io_error
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
input_key:
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
    ld h,#1
    ld l,#0
    call input_syscall
    or a
    jp nz,input_fail
    ld a,b
    or a
    jp nz,input_fail
    ld a,c
    or a
    jr z,input_key
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
    ld a,#255
    ld (echo_fd),a
    ld bc,#input_video_name
    ld h,#1
    ld l,#2
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
    ld h,#1
    ld l,#7
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
    call input_cleanup
    jp io_error
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
    ld hl,(line_next)
    ld a,(line_remaining)
    cp c
    jr c,read_string_length
    ld a,c
read_string_length:
    ld (de),a
    inc de
    ld c,a
    ld b,#0
    or a
    ret z
    ld a,(line_remaining)
    sub c
    ld (line_remaining),a
    ldir
    ld (line_next),hl
    ret
input_end::
    ld a,(io_status)
    or a
    jp nz,io_check
    ld a,(line_valid)
    or a
    call z,input_ready
    xor a
    ld (line_remaining),a
    ld (line_valid),a
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
    ld a,(line_remaining)
    or a
    jr z,input_terminator
    ld hl,(line_next)
    ld a,(hl)
    ld (de),a
    jp input_advance
input_terminator:
    ld a,#2
    ld (line_valid),a
    ; A buffered console line is terminated by Ctrl-Z, including ordinary EOL.
    ; Consuming that marker permits the next field to refill the buffer.
    ld a,#26
    ld (de),a
    ret
input_advance:
    ld hl,(line_next)
    inc hl
    ld (line_next),hl
    ld a,(line_remaining)
    dec a
    ld (line_remaining),a
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
    ld a,(line_remaining)
    or a
    jr z,input_token_begin
    ld hl,(line_next)
    ld a,(hl)
    cp #33
    jr nc,input_token_begin
    call input_advance
    jr input_skip
input_token_begin:
    ld de,#input_token+1
    ld c,#0
input_token_byte:
    ld a,(line_remaining)
    or a
    jr z,input_token_ready
    ld hl,(line_next)
    ld a,(hl)
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
