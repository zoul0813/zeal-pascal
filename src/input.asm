.module input
.globl read_line,read_string,input_begin,input_value,input_end
.globl memory_mode,io_error,call_error,val_dispatch
.area _REAL_DATA
line_next: .ds 2
line_remaining: .ds 1
line_valid: .ds 1               ; 0 unloaded, 1 buffered, 2 end marker consumed
line_eof: .ds 1
line_buffer: .ds 255
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
; Retain the current Zeal cooked editor until full Turbo console controls land.
; The entire physical line is read before fields are copied, bounded to127.
read_line::
    ld h,#1
    ld l,#7
    ld c,#0
    ld de,#1                    ; blocking cooked
    call input_syscall
    or a
    jp nz,io_error
    ld de,#line_buffer
    ld bc,#255
    ld h,#1
    ld l,#0
    call input_syscall
    push af
    push bc
    ld a,(memory_mode)
    or a
    ld de,#0                    ; parent IDE: blocking raw
    jr nz,read_restore
    ld e,#4                     ; ZShell: nonblocking raw
read_restore:
    ld h,#1
    ld l,#7
    ld c,#0
    call input_syscall
    pop bc
    pop de                      ; D = read error, E = saved flags
    or d
    jp nz,io_error
    ld a,b
    or a
    jp nz,io_error
    ld hl,#line_buffer
    ld (line_next),hl
    ld b,c
    ld c,#0
    ld a,b
    or a
    jr z,read_eof
read_line_length:
    ld a,(hl)
    cp #26
    jr z,read_eof
    cp #10
    jr z,read_line_ready
    cp #13
    jr z,read_line_ready
    ld a,c
    cp #127
    jr z,read_line_ready
    inc c
    inc hl
    djnz read_line_length
    jr read_line_ready
read_eof:
    ld a,#1
    ld (line_eof),a
read_line_ready:
    ld a,c
    ld (line_remaining),a
    ld a,#1
    ld (line_valid),a
    ret
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
    ld a,(line_valid)
    or a
    call z,input_ready
    xor a
    ld (line_remaining),a
    ld (line_valid),a
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
    ld b,#3
    jp call_error
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
