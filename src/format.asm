.module format
.globl format_field,field_output,str_copy,integer_prepare
.globl format_real,character,neg_hl,false_text,true_text
.area _REAL_DATA
field_text: .ds 8
.area _CODE
; Pure field preparation. HL points to places,width,value words; A is the
; compiler's scalar family. No callback/syscall occurs before the caller copies
; the returned short string into its invocation-owned temporary.
format_field::
    cp #5
    jr z,field_real
    inc hl
    inc hl
    inc hl
    inc hl
    ld e,(hl)
    inc hl
    ld d,(hl)
    ex de,hl
    cp #1
    ret z
    cp #2
    jr z,field_boolean
    cp #3
    jp nz,integer_prepare
    ld a,l
    ld (field_text+1),a
    ld hl,#field_text
    ld (hl),#1
    ret
field_boolean:
    ld a,h
    or l
    ld hl,#false_text
    ret z
    ld hl,#true_text
    ret
field_real:
    ld e,(hl)
    inc hl
    ld d,(hl)
    inc hl
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    ld a,(hl)
    inc hl
    ld h,(hl)
    ld l,a
    jp format_real

; HL signed word -> short string. Conversion is synchronous, with the sign
; retained on the stack rather than shared output state. All Integer output
; and Str use this one conversion, including -32768.
integer_prepare::
    xor a
    bit 7,h
    jr z,integer_magnitude
    call neg_hl
    ld a,#1
integer_magnitude:
    push af
    ld de,#field_text+7
integer_digit:
    push de
    ld de,#10
    ld bc,#0
integer_divide:
    or a
    sbc hl,de
    jr c,integer_remainder
    inc bc
    jr integer_divide
integer_remainder:
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
    jr nz,integer_digit
    pop af
    or a
    jr z,integer_length
    dec de
    ld a,#45
    ld (de),a
integer_length:
    ld hl,#field_text+7
    or a
    sbc hl,de
    dec de
    ld a,l
    ld (de),a
    ex de,hl
    ret

; Widths below zero give no padding; positive widths above255 saturate at255,
; as in the original. HL short string -> DE nonnegative padding. Both
; callers own the source bytes. BC survives for Str's destination pointer.
field_padding:
    bit 7,d
    jr nz,field_no_padding
    ld a,d
    or a
    jr z,field_width_ready
    ld de,#255
field_width_ready:
    push bc
    ld c,(hl)
    ld b,#0
    push hl
    ex de,hl
    or a
    sbc hl,bc
    jr nc,field_padding_ready
    ld hl,#0
field_padding_ready:
    ex de,hl
    pop hl
    pop bc
    ret
field_no_padding:
    ld de,#0
    ret

; HL owned short string, DE signed minimum width. The active source PC and
; iteration state survive every character call on this invocation's stack.
field_output::
    call field_padding
    ld bc,(0x40be)
field_spaces:
    ld a,d
    or e
    jr z,field_body
    ld a,#32
    call field_character
    dec de
    jr field_spaces
field_body:
    ld e,(hl)
    ld d,#0
    inc hl
field_bytes:
    ld a,d
    or e
    ret z
    ld a,(hl)
    call field_character
    inc hl
    dec de
    jr field_bytes
field_character:
    push hl
    push de
    push bc
    ld (0x40be),bc
    call character
    pop bc
    pop de
    pop hl
    ret

; HL owned numeric body, DE String destination, BC signed width, A capacity.
; Write only the prefix that fits: a huge field may consist entirely of spaces.
; No 255-byte expression or device output is manufactured before truncation.
str_copy::
    push af
    push de
    ld d,b
    ld e,c
    call field_padding
    pop bc
    pop af
    push af
    ld a,d
    or a
    jr nz,str_all_spaces
    pop af
    cp e
    jr c,str_spaces_only
    jr z,str_spaces_only
    sub e
    cp (hl)
    jr c,str_length_ready
    ld a,(hl)
str_length_ready:
    ld d,a
    add a,e
    ld (bc),a
    inc bc
str_padding:
    ld a,e
    or a
    jr z,str_body
    ld a,#32
    ld (bc),a
    inc bc
    dec e
    jr str_padding
str_body:
    inc hl
str_bytes:
    ld a,d
    or a
    ret z
    ld a,(hl)
    ld (bc),a
    inc hl
    inc bc
    dec d
    jr str_bytes
str_all_spaces:
    pop af
str_spaces_only:
    ld d,a
    ld (bc),a
    inc bc
str_fill:
    ld a,d
    or a
    ret z
    ld a,#32
    ld (bc),a
    inc bc
    dec d
    jr str_fill
