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
    jp compare_integer          ; 40CC: DE left, HL right; -1/0/+1, DE preserved
    jp compare_string           ; 40CF: DE left, HL right short-string pointers
    jp boolean_output           ; 40D2: HL normalized Boolean
    jp shift_left               ; 40D5: DE value, HL count
    jp shift_right              ; 40D8: DE value, HL count
    jp check_range              ; 40DB: DE lower, BC upper, HL ordinal
    jp character_copy           ; 40DE: L Char, DE short-string destination
    jp string_character         ; 40E1: HL short string -> zero-extended Char
    jp compare_text             ; 40E4: DE left, HL right, A Char operand flags
    jp push_string              ; 40E7: push current length+1 value argument

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

boolean_output:
    ld a,h
    or l
    ld hl,#false_text
    jp z,string_output
    ld hl,#true_text
    jp string_output

; Bias each signed high byte before the unsigned lexicographic comparison.
; No subtract-and-sign shortcut: -32768 versus +32767 must not overflow.
compare_integer:
    ld a,d
    xor #0x80
    ld b,a
    ld a,h
    xor #0x80
    ld c,a
    ld a,b
    cp c
    jr c,compare_less
    jr nz,compare_greater
    ld a,e
    cp l
    jr c,compare_less
    jr nz,compare_greater
compare_equal:
    ld hl,#0
    ret
compare_less:
    ld hl,#0xffff
    ret
compare_greater:
    ld hl,#1
    ret
check_range:
    push hl
    push bc
    call compare_integer        ; lower > value is invalid
    ld a,l
    cp #1
    jr z,range_error
    pop de
    pop hl
    push hl
    ex de,hl
    call compare_integer        ; value > upper is invalid
    ld a,l
    cp #1
    jr z,range_error
    pop hl
    ret
range_error:
    ld a,#0x91
    ld b,#2
    jp call_error
character_copy:
    ld a,#1
    ld (de),a
    inc de
    ld a,l
    ld (de),a
    ret
string_character:
    ld a,(hl)
    cp #1
    jr nz,character_length_error
    inc hl
    ld l,(hl)
    ld h,#0
    ret
character_length_error:
    ld a,#0x10
    ld b,#2
    jp call_error
compare_text:
    ld c,a
    bit 0,c
    jr z,text_right
    ld b,e
    ld de,#text_left_char
    ld a,#1
    ld (de),a
    inc de
    ld a,b
    ld (de),a
    dec de
text_right:
    bit 1,c
    jr z,compare_string
    ld b,l
    ld hl,#text_right_char
    ld (hl),#1
    inc hl
    ld (hl),b
    dec hl
compare_string:
    ld a,(de)
    ld b,a
    ld c,(hl)
    inc de
    inc hl
compare_string_loop:
    ld a,b
    or a
    jr nz,compare_string_left
    ld a,c
    or a
    jr z,compare_equal
    jr compare_less
compare_string_left:
    ld a,c
    or a
    jr z,compare_greater
    ld a,(de)
    cp (hl)
    jr c,compare_less
    jr nz,compare_greater
    inc de
    inc hl
    dec b
    dec c
    jr compare_string_loop

shift_left:
    ld a,#0
    jr shift_start
shift_right:
    ld a,#1
shift_start:
    ld (shift_direction),a
    ld a,h
    or a
    jr nz,shift_zero
    ld a,l
    cp #16
    jr nc,shift_zero
    ld b,a
    ex de,hl
    or a
    ret z
    ld a,(shift_direction)
    or a
    jr nz,shift_right_loop
shift_left_loop:
    add hl,hl
    djnz shift_left_loop
    ret
shift_right_loop:
    srl h
    rr l
    djnz shift_right_loop
    ret
shift_zero:
    ld hl,#0
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
false_text: .db 5
    .ascii "FALSE"
true_text: .db 4
    .ascii "TRUE"
shift_direction: .db 0
text_left_char: .ds 2
text_right_char: .ds 2

; A value string actual has its current size on the Pascal stack. The callee
; copies it into its formal capacity. The return word is held outside that
; new argument, and no user callback/syscall occurs inside this helper.
push_string:
    pop bc
    ld (argument_return),bc
    ld (argument_source),hl
    ld e,(hl)
    ld d,#0
    inc de
    ld hl,#0xffc0
    add hl,sp
    jr nc,argument_overflow
    or a
    sbc hl,de
    jr c,argument_overflow
    ld a,h
    cp #0xc8
    jr c,argument_overflow
    ld hl,#0
    add hl,sp
    or a
    sbc hl,de
    ld sp,hl
    ld b,d
    ld c,e
    ex de,hl
    ld hl,(argument_source)
    ldir
    ld hl,(argument_return)
    jp (hl)
argument_overflow:
    ld hl,(0x40be)
    jp stack_error
argument_return: .dw 0
argument_source: .dw 0
