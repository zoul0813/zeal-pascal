.module runtime
.area _HEADER (ABS)
.org 0x4000
    jp 0x40a0
    .ascii "ZP80"
    .db 1
    .dw 160,0,0x40a0,0x40a3,1,0,0,0,0x8000,0,0,0xfbff,2048,0
    .ds 96
    .dw 16384,0,0,0
    .ds 20
.org 0x40a0
    jp normal
    jp ram
    jp done
    jp halt
    jp character
    jp integer
    jp divide
    jp modulo
    jp multiply
    jp stack_error
    .dw 0                       ; private checked helper call-site PC, 40BE
    jp string_output            ; 40C0: HL -> short string
    jp string_copy              ; 40C3: HL source, DE destination, C capacity
    jp read_line                ; 40C6: fresh default console line
    jp read_string              ; 40C9: DE destination, C capacity

; This prefix is deliberately separate from the C compiler's ABI. Only the
; private helper entries above are emitted in this first language increment.
normal:
    ld a,b
    or a
    jr nz,bad_start
    ld a,c
    cp #128
    jr nc,bad_start
    ld a,c
    ld (tail_length),a
    or a
    jr z,tail_copied
    ex de,hl
    ld de,#tail
    ldir
tail_copied:
    xor a
    ld (memory_mode),a
    ld sp,#0xfc00
    jp start
bad_start:
    ld h,#1
    ld l,#15
    rst 8
ram:
    ld a,#1
    ld (memory_mode),a
start:
    ld hl,(0x4014)
    jp (hl)
done:
    ld a,#1
    jr finish
halt:
    ld a,#4
finish:
    ld hl,#memory_mode
    bit 0,(hl)
    jr z,os_exit
    ld (0xfc1a),a
    jp 0xfd80
os_exit:
    ld h,#0
    ld l,#15
    rst 8

; All helper paths use fewer than 64 bytes including their syscall entry.
; The compiler checks that complete budget before pushing/calling them.
character:
    ld (io_byte),a
    ld de,#io_byte
    ld bc,#1
    ld h,#0
    ld l,#1
    rst 8
    or a
    jr nz,io_error
    ld a,b
    or a
    jr nz,io_error
    ld a,c
    cp #1
    ret z
io_error:
    ld a,#0xf0
    ld b,#3
    jp call_error

string_output:
    ld b,(hl)
    inc hl
    ld a,b
    or a
    ret z
string_output_loop:
    ld a,(hl)
    push hl
    push bc
    call character
    pop bc
    pop hl
    inc hl
    djnz string_output_loop
    ret

string_copy:
    ld a,(hl)
    inc hl
    cp c
    jr c,string_copy_length
    ld a,c
string_copy_length:
    ld (de),a
    inc de
    ld c,a
    ld b,#0
    or a
    ret z                       ; never LDIR with BC=0
    ldir                        ; self-assignment is safe, too
    ret

; Use the native driver's supported cooked line editor for this increment.
; Read a whole physical line before copying fields, so a short destination
; does not leave the line tail to satisfy the next ReadLn. Full Turbo console
; controls/BufLen and the exact 127-key editing boundary remain a later gate.
read_line:
    ld h,#1
    ld l,#7
    ld c,#0
    ld de,#1                    ; blocking cooked
    rst 8
    or a
    jp nz,io_error
    ld de,#line_buffer
    ld bc,#255
    ld h,#1
    ld l,#0
    rst 8
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
    rst 8
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
    jr z,read_line_ready
read_line_length:
    ld a,(hl)
    cp #10
    jr z,read_line_ready
    cp #13
    jr z,read_line_ready
    cp #26
    jr z,read_line_ready
    ld a,c
    cp #127
    jr z,read_line_ready
    inc c
    inc hl
    djnz read_line_length
read_line_ready:
    ld a,c
    ld (line_remaining),a
    ret
read_string:
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
divide:
    ld a,#0
    jr divmod
modulo:
    ld a,#1
divmod:                         ; DE dividend, HL divisor; quotient-sign MOD
    ld (want_remainder),a
    ld a,h
    or l
    jr nz,nonzero
    ld a,#2
    ld b,#2
    jp call_error
nonzero:
    ld a,d
    xor h
    and #0x80
    ld (negative),a
    bit 7,h
    call nz,neg_hl
    ex de,hl
    bit 7,h
    call nz,neg_hl
    ld b,h
    ld c,l
    ld hl,#0
    ld a,#16
divide_loop:
    sla c
    rl b
    adc hl,hl
    or a
    sbc hl,de
    jr nc,divide_bit
    add hl,de
    jr divide_next
divide_bit:
    inc c
divide_next:
    dec a
    jr nz,divide_loop
    ld a,(want_remainder)
    or a
    jr nz,div_sign
    ld h,b
    ld l,c
div_sign:
    ld a,(negative)
    or a
    ret z
neg_hl:
    xor a
    sub l
    ld l,a
    sbc a,a
    sub h
    ld h,a
    ret
multiply:
    ld b,h
    ld c,l
    ld hl,#0
    ld a,#16
mul_loop:
    srl b
    rr c
    jr nc,mul_skip
    add hl,de
mul_skip:
    sla e
    rl d
    dec a
    jr nz,mul_loop
    ret

integer:
    bit 7,h
    jr z,positive
    call neg_hl
    push hl
    ld a,#45
    call character
    pop hl
positive:
    ld de,#digits+5
    xor a
    ld (de),a
decimal_loop:
    push de
    ld de,#10
    ld bc,#0
decimal_sub:
    or a
    sbc hl,de
    jr c,decimal_ready
    inc bc
    jr decimal_sub
decimal_ready:
    add hl,de
    ld a,l
    add a,#48
    pop de
    dec de
    ld (de),a
    ld h,b
    ld l,c
    ld a,h
    or l
    jr nz,decimal_loop
decimal_print:
    ld a,(de)
    or a
    ret z
    push de
    call character
    pop de
    inc de
    jr decimal_print

call_error:                     ; preserve user call site, never resume
    ld (error_code),a
    ld a,b
    ld (error_namespace),a
    ld hl,(0x40be)
    jr error_ready
stack_error:                    ; HL already contains compiler call-site PC
    ld a,#0xff
    ld (error_code),a
    ld a,#2
    ld (error_namespace),a
error_ready:
    ld sp,#0xfc00               ; stop using faulting stack; no resumption
    ld (0xfc1e),hl
    ld a,(memory_mode)
    or a
    jp z,normal_error
    ld a,#3
    ld (0xfc1a),a
    ld a,(error_namespace)
    ld (0xfc1b),a
    ld a,(error_code)
    ld (0xfc1c),a
    xor a
    ld (0xfc1d),a
    ld a,#2
    ld (0xfc21),a
    jp 0xfd80

normal_error:
    ld a,(error_code)
    ld h,a
    ld l,#15
    rst 8

memory_mode: .db 0
error_code: .db 0
error_namespace: .db 0
io_byte: .db 0
negative: .db 0
want_remainder: .db 0
digits: .ds 6
tail_length: .db 0
tail: .ds 127
line_next: .dw 0
line_remaining: .db 0
line_buffer: .ds 255
