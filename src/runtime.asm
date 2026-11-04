.module runtime
.globl real_dispatch,real_buffer,call_error
.globl format_field,field_output,str_copy,integer_prepare
.globl val_field
.globl read_line,read_string,input_begin,input_value,input_end,memory_mode,io_error
.globl buflen,cbreak,user_break
.globl input_mode_value
.globl input_reset,io_frame
.globl input_state_begin,input_reset_count,echo_fd
.globl _key_reset
.globl normal_error,error_code,error_namespace
.globl runtime_prefix_end
.globl character,neg_hl,false_text,true_text
.globl io_call,io_result,io_check,io_fail,io_status
.globl tail_length,tail
.globl runtime_stack_budget,recursion_ceiling
.globl file_cleanup,heap_floor,io_status
.equ tail_length, 0xff00
.equ tail, 0xff01
.equ runtime_stack_budget, 0xfe7c
.equ recursion_ceiling, 0xfd64  ; existing private guard scratch word
.area _HEADER (ABS)
.org 0x4000
    jp 0x40a0
    .ascii "ZP80"
    .db 2
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
    jp argument_string          ; 40EA: consume fixed temporary as value argument
    jp hold_string              ; 40ED: snapshot HL into a fixed 256-byte slot
    jp concatenate              ; 40F0: DE left, HL right, A Char flags, BC owned bytes
    jp check_index              ; 40F3: DE lower, BC upper, HL index; error90
    jp substring                ; 40F6: HL -> count/position/source words, DE release bytes
    jp position                 ; 40F9: DE pattern, HL source -> HL first position
    jp delete_string            ; 40FC: HL destination, BC position, DE count
    jp insert_string            ; 40FF: HL source, DE destination, BC position, A capacity
    jp move_bytes               ; 4102: HL source, DE destination, BC bytes; overlap safe
    jp set_zero                 ; 4105: clear 32 bytes at HL
    jp set_load                 ; 4108: expand HL packed set into reserved DE slot, A first byte, C width
    jp set_store                ; 410B: HL full set -> DE packed set, A first byte, C width
    jp set_add                  ; 410E: HL lower, DE upper, BC full-set address
    jp set_binary               ; 4111: DE left, HL right, A operation, BC owned bytes
    jp set_compare              ; 4114: DE left, HL right, A comparison -> Boolean HL
    jp set_member               ; 4117: DE ordinal, HL full set -> Boolean HL
    jp checked_guard            ; 411A: inline reservation and source-PC words
    jp heap_allocate            ; 411D: HL bytes -> HL pointer
    jp heap_free                ; 4120: HL pointer, DE original bytes
    jp heap_release             ; 4123: HL prior Mark frontier
    jp heap_availability        ; 4126: A=0 total, 1 largest, 2 frontier
    jp stack_set                ; 4129: HL validated new CPU SP
heap_frontier: .dw 0            ; 412C: predefined HeapPtr
recursion_limit: .dw 0          ; 412E: TP3 public recursion-stack pointer
    jp fill_bytes               ; 4130: HL destination, BC count, A byte
    jp real_dispatch            ; 4133: DE left, HL right, A op/Integer flags
real_buffer: .ds 6              ; 4136: transient helper/return bytes, copied by caller
    jp format_field             ; 413C: HL field record, A family -> short-string body
    jp field_output             ; 413F: HL owned body, DE signed minimum field width
    jp str_copy                 ; 4142: HL body, DE destination, BC width, A capacity
    jp val_field                ; 4145: HL Code/number/source record, A Val operation
    jp input_begin              ; 4148: A fresh/default Input policy; no device read
    jp input_value              ; 414B: HL destination, B scalar kind, C String capacity
    jp input_end                ; 414E: ReadLn discards through current line end
buflen: .db 127                 ; 4151: next physical console line's byte limit
cbreak: .db 1                   ; 4152: buffered Ctrl-C checking enabled
    jp io_call                  ; 4153: inline helper:u16, I checking:u8
    jp io_result                ; 4156: Integer prior I/O status, cleared
    jp file_cleanup             ; 4159: A lifetime operation, B checking, DE owner, HL end
heap_floor: .dw 0               ; 415C: actual public heap origin
    jp io_fail                  ; 415E: generated file operation's I/O failure
    jp call_error               ; 4161: A code, B namespace
io_status: .db 0                ; 4164: pending I/O error, consumed by IOResult
io_frame: .dw 0                 ; invocation-owned stack frame, never a global landing
io_target: .dw 0                ; setup/dispatch scratch; no callback while in use

; This prefix is deliberately separate from the C compiler's ABI. Only the
; private helper entries above are emitted in this first language increment.
normal:
    ld a,b
    or a
    jr nz,bad_start
    bit 7,c
    jr nz,bad_start
    ld a,c
    ex af,af'                   ; retain length without touching source/stack
    ld a,c
    or a
    jr z,tail_copied
    ex de,hl
    ld de,#tail
    ; The OS tail can overlap its private destination. Copy toward the lower
    ; end when source >= destination, otherwise toward the higher end. Store
    ; the length only after copying: even FF00 may be a source byte.
    ld a,h
    inc a
    jr nz,tail_backward
    ld a,l
    or a
    jr z,tail_backward
    ldir
    jr tail_copied
tail_backward:
    add hl,bc
    dec hl
    ex de,hl
    add hl,bc
    dec hl
    ex de,hl
    lddr
tail_copied:
    ex af,af'
    ld (tail_length),a
    xor a
    ld (memory_mode),a
    ld sp,#0xfc00
    jr start
bad_start:
    ld h,#1
    ld l,#15
    rst 8
ram:
    xor a
    ld (tail_length),a
    ld a,#1
    ld (memory_mode),a
start:
    ld ix,#0                    ; terminate the actual Pascal frame chain
    xor a
    ld (io_status),a
    ld (error_code),a
    ld (error_namespace),a
    call input_reset
    call _key_reset
    ld hl,#0
    ld (recursion_ceiling),hl
    ld (io_frame),hl
    ld hl,#0xfe70               ; runtime-owned persistent module control
    ld de,#0xfe71
    ld bc,#31
    ld (hl),#0
    ldir
    ld a,(memory_mode)
    ld (0xfe78),a
    xor #1
    rlca
    rlca
    ld (input_mode_value),a     ; owned raw mode: IDE blocking, shell nonblocking
    ld hl,(0x4020)              ; minimum load-time stack, never below2 KiB
    ld a,h
    cp #8
    jp c,heap_error
    ld (runtime_stack_budget),hl
    ld hl,#0xff80               ; input history, initialized after tail copy
    ld de,#0xff81
    ld bc,#127
    ld (hl),#0
    ldir
    ld hl,(0x401c)
    inc hl
    res 0,l
    bit 7,h                    ; reject prefix overlap and FFFF alignment wrap
    jp z,heap_error
heap_aligned:
    ld (heap_floor),hl
    ld (heap_frontier),hl
    ld hl,(0x401e)
    inc hl                     ; selected End+1 must stay in page3
    ld a,h
    cp #0xd0
    jp c,heap_error
    call stack_set             ; validate End ceiling and actual static headroom
    ld hl,#64
    ld (runtime_stack_budget),hl
    ld hl,#-1024                 ; documented initial1 KiB CPU/recursion split
    add hl,sp
    ld (recursion_limit),hl
    ld hl,(0x4014)
    jp (hl)
done:
    ld a,#1
    jr finish
halt:
    ld a,#4
finish:
    ld sp,#0xfc00
    push af
    ld a,#19
    ld de,#0
    ld hl,#0
    call file_cleanup
    pop af
    ld hl,#memory_mode
    bit 0,(hl)
    jr z,os_exit
    ld (0xfc1a),a
    jp 0xfd80
os_exit:
    ld h,#0
    ld l,#15
    rst 8

; Input owns and releases its console resources before this nonlocal return.
user_break:
    ld sp,#0xfc00
    ld b,#2                    ; preserve this primary break through cleanup
    ld a,#19
    ld de,#0
    ld hl,#0
    call file_cleanup
    ld a,(memory_mode)
    or a
    jr z,normal_break
    ld hl,(0x40be)
    ld sp,#0xfc00
    ld (0xfc1e),hl
    ld a,#5
    ld (0xfc1a),a
    xor a
    ld (0xfc1b),a
    ld (0xfc1c),a
    ld (0xfc1d),a
    inc a
    ld (0xfc20),a
    ld a,#2
    ld (0xfc21),a
    jp 0xfd80
normal_break:
    ld h,#1
    ld l,#15
    rst 8

; All helper paths use fewer than 64 bytes including their syscall entry.
; The compiler checks that complete budget before pushing/calling them.
character:
    ld (io_byte),a
    ld a,(io_status)
    or a
    jp nz,io_check
    ld hl,(0x40be)
    push hl                     ; restore this invocation's source PC after I/O
    ld de,#io_byte
    ld bc,#1
    ld h,#0
    ld l,#1
    rst 8
    pop hl
    ld (0x40be),hl
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
    jp io_fail

; Wrap exactly one runtime I/O operation, after its argument expressions.
; Preserve all incoming operands while inserting four invocation-owned bytes
; below the caller's return word. No callback/syscall occurs during setup.
; Claim the lower destination before moving the saved registers: interrupts
; may push below SP at any instruction, including between LDIR iterations.
; After restoration the stack is {checking:u16, previous_frame:u16, return_PC:u16}.
io_call:
    push af
    ld a,(io_status)
    or a
    jr nz,io_detail_ready
    ld (0xfe7a),a
    ld (0xfe7b),a
io_detail_ready:
    push bc
    push de
    push hl
    ld hl,#8
    add hl,sp
    ld e,(hl)
    inc hl
    ld d,(hl)
    ex de,hl
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    ld (io_target),bc
    ld a,(hl)
    inc hl
    ex de,hl
    ld (hl),d
    dec hl
    ld (hl),e
    ld hl,#-4
    add hl,sp
    ld sp,hl
    ex de,hl
    ld hl,#4
    add hl,sp
    ld bc,#8
    ldir
    ld hl,(io_frame)
    ld (io_frame),de
    ld (de),a
    inc de
    xor a
    ld (de),a
    inc de
    ld a,l
    ld (de),a
    inc de
    ld a,h
    ld (de),a
    pop hl
    pop de
    pop bc
    pop af
    push hl
    ld hl,#io_return
    ex (sp),hl
    push hl
    ld hl,(io_target)
    ex (sp),hl
    ret
io_return:
    pop bc
    pop hl
    ld (io_frame),hl
    ret
io_fail:
    ld (io_status),a
io_check:
    ld hl,(io_frame)
    ld a,h
    or l
    jr z,io_checked
    ld a,(hl)
    or a
    jr nz,io_checked
    ld sp,hl
    jr io_return
io_checked:
    ld a,(io_status)
    ld b,#3
    jp call_error
io_result:
    ld a,(io_status)
    ld l,a
    ld h,#0
    xor a
    ld (io_status),a
    ld (0xfe7a),a
    ld (0xfe7b),a
    ret

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
    jr z,string_output
    ld hl,#true_text
    jr string_output

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
    ld a,#0x91
    jr check_ordinal
check_index:
    ld a,#0x90
check_ordinal:
    ld (range_error_code),a
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
    ld a,(range_error_code)
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
    call prepare_text
    jr compare_string
prepare_text:
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
    ret z
    ld b,l
    ld hl,#text_right_char
    ld (hl),#1
    inc hl
    ld (hl),b
    dec hl
    ret
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
    jp c,compare_less
    jp nz,compare_greater
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
    call integer_prepare
    push ix
    ld ix,#0
    add ix,sp
    ex de,hl
    ld hl,#-8
    add hl,sp
    ld sp,hl
    ex de,hl
    ld c,(hl)
    ld b,#0
    inc bc
    ldir
    ld hl,#0
    add hl,sp
    call string_output
    ld sp,ix
    pop ix
    ret

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
    ld b,#2                    ; preserve the primary runtime/I/O fault
    ld a,#19
    ld de,#0
    ld hl,#0
    call file_cleanup
    ld a,(memory_mode)
    or a
    jr z,normal_error
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
.equ io_byte, 0xfe82
.equ negative, 0xfe80
.equ want_remainder, 0xfe81
false_text: .db 5
    .ascii "FALSE"
true_text: .db 4
    .ascii "TRUE"
.equ shift_direction, 0xfe83
.equ range_error_code, 0xfe84
.equ text_left_char, 0xfe85
.equ text_right_char, 0xfe87

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
    call check_boundary
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
    jr string_return
argument_overflow:
    ld hl,(0x40be)
    jp stack_error
; Synchronous String scratch shares heap work words. Neither subsystem
; invokes the other while these words are live.
.equ argument_return, 0xfd66
.equ argument_source, 0xfd68

; A function/concatenation owns a fixed maximum-size result slot. A value
; actual must occupy only its current length+1 bytes, without leaving padding
; between earlier actuals and this one. BC is the fixed slot's owned size.
argument_string:
    ld (argument_source),hl
    pop de
    ld (argument_return),de
    ld e,(hl)
    ld d,#0
    inc de
    ld h,b
    ld l,c
    or a
    sbc hl,de
    jp c,character_length_error
    ld hl,#0
    add hl,sp
    add hl,bc
    or a
    sbc hl,de
    ld sp,hl
    ex de,hl
    ld b,h
    ld c,l
    ld hl,(argument_source)
    jr move_string_tail

hold_string:
    ld (argument_source),hl
    pop bc
    ld (argument_return),bc
    ld hl,#0xfec0              ; prospective SP minus 256 and emergency 64
    add hl,sp
    jr nc,argument_overflow
    call check_boundary
    ld hl,#0xff00
    add hl,sp
    ld sp,hl
    ex de,hl
    ld hl,(argument_source)
    ld c,(hl)
    ld b,#0
    inc bc
    ldir
string_return:
    ld hl,#0
    add hl,sp
    ld de,(argument_return)
    push de                    ; guarded emergency space, below result bytes
    ret

; Moving upward can overlap. No push/call may overwrite the source after
; changing SP and before LDDR completes. BC is always length+1, never zero.
move_string_tail:
    add hl,bc
    dec hl
    ex de,hl
    add hl,bc
    dec hl
    ex de,hl
    lddr
    jr string_return

concatenate:
    ld (concatenate_drop),bc
    call prepare_text
    ld (concatenate_left),de
    ld (concatenate_right),hl
    pop hl
    ld (concatenate_return),hl
    ld hl,(concatenate_left)
    call hold_string
    ld a,(hl)
    ld (concatenate_length),a
    ld de,(concatenate_right)
    ld a,(de)
    ld c,a
    ld b,#0
    ld a,(concatenate_length)
    add a,c
    jp c,character_length_error
    ld (hl),a
    ld a,(concatenate_length)
    ld e,a
    ld d,#0
    inc de
    add hl,de
    ex de,hl
    ld hl,(concatenate_right)
    inc hl
    ld a,c
    or a
    jr z,concatenate_ready
    ldir
concatenate_ready:
    ld hl,(concatenate_return)
    ld (argument_return),hl
    ld de,(concatenate_drop)
finish_string:
    ld hl,#0
    add hl,sp
    ld (argument_source),hl
    ld c,(hl)
    ld b,#0
    inc bc
    add hl,de
    ld sp,hl
    ex de,hl
    ld hl,(argument_source)
    jr move_string_tail
.equ concatenate_drop, 0xfd6a
.equ concatenate_left, 0xfd6c
.equ concatenate_right, 0xfd6e
.equ concatenate_return, 0xfd70
.equ concatenate_length, 0xfd72

; Positions/counts are checked independently of $R. No character storage is
; accessed by these operations until the argument bounds have been accepted.
string_bounds:
    bit 7,d
    jr nz,string_position_error
string_position_bound:
    ld a,b
    or a
    jr nz,string_position_error
    ld a,c
    or a
    ret nz
string_position_error:
    ld a,#0x11
    ld b,#2
    jp call_error
string_clip:                    ; A remaining, DE nonnegative request -> C clipped
    ld c,a
    ld a,d
    or a
    ret nz
    ld a,e
    cp c
    ret nc
    ld c,e
    ret
substring:
    ld (string_drop),de
    pop bc
    ld (argument_return),bc
    ld e,(hl)
    inc hl
    ld d,(hl)
    inc hl
    ld (string_count),de
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    ld (string_position),bc
    ld e,(hl)
    inc hl
    ld d,(hl)
    ld (string_source),de
    ld de,(string_count)
    call string_bounds
    ld a,d
    or e
    jr z,substring_empty
    ld hl,(string_source)
    ld a,(hl)
    cp c
    jr c,substring_empty
    sub c
    inc a
    call string_clip
    ld de,(string_position)
    add hl,de
    ex de,hl
    ld hl,#0
    add hl,sp
    ld (hl),c
    inc hl
    ex de,hl
    ld b,#0
    ld a,c
    or a
    jr z,substring_ready
    ldir
    jr substring_ready
substring_empty:
    ld hl,#0
    add hl,sp
    ld (hl),#0
substring_ready:
    ld de,(string_drop)
    jp finish_string
position:
    ld a,(de)
    or a
    jr z,position_none
    ld b,a                     ; pattern length, always nonzero
    ld a,(hl)
    sub b
    jr c,position_none
    inc a
    ld c,a                     ; number of candidate starts,1..255
    inc hl
    inc de
    ld a,#1                    ; one-based candidate index
position_candidate_loop:
    push af
    push bc
    push hl
    push de
position_compare_loop:
    ld a,(de)
    cp (hl)
    jr nz,position_next
    inc de
    inc hl
    djnz position_compare_loop
    pop de
    pop hl
    pop bc
    pop af
    ld l,a
    ld h,#0
    ret
position_next:
    pop de
    pop hl
    pop bc
    pop af
    inc hl
    inc a
    dec c
    jr nz,position_candidate_loop
position_none:
    ld hl,#0
    ret
delete_string:
    ld (string_position),bc
    call string_bounds
    ld a,d
    or e
    ret z
    ld a,(hl)
    cp c
    ret c
    sub c
    inc a
    ld (string_remaining),a
    call string_clip
    ld a,(hl)
    sub c
    ld (hl),a
    ld de,(string_position)
    add hl,de
    ex de,hl
    ld l,c
    ld h,#0
    add hl,de
    ld a,(string_remaining)
    sub c
    ld c,a
    ld b,#0
    or a
    ret z
    ldir
    ret
insert_string:
    ld (string_capacity),a
    ld (string_source),hl
    ld (string_destination),de
    call string_position_bound
    ld a,(hl)
    or a
    ret z
    ld hl,(string_destination)
    ld a,c
    dec a
    cp (hl)
    jr c,insert_prefix
    ld a,(hl)
insert_prefix:
    ld (string_prefix),a
    ld c,a
    ld a,(string_capacity)
    sub c
    ld c,a                     ; available after prefix
    ld hl,(string_source)
    ld a,(hl)
    cp c
    jr c,insert_count
    ld a,c
insert_count:
    ld (string_inserted),a
    ld b,a
    ld a,c
    sub b                      ; remaining capacity for the old tail
    ld c,a
    ld hl,(string_destination)
    ld a,(string_prefix)
    ld b,a
    ld a,(hl)
    sub b
    cp c
    jr c,insert_tail
    ld a,c
insert_tail:
    ld (string_tail),a
    add a,b
    ld c,a
    ld a,(string_inserted)
    add a,c
    ld (hl),a                  ; final bounded length
    ld a,(string_tail)
    or a
    jr z,insert_source
    ld c,a
    ld b,#0
    ld a,(string_prefix)
    add a,c
    ld e,a
    ld d,#0
    add hl,de                  ; old tail's last retained character
    push hl
    ld a,(string_inserted)
    ld e,a
    add hl,de
    ex de,hl
    pop hl
    lddr                       ; right shift can overlap
insert_source:
    ld a,(string_inserted)
    or a
    ret z
    ld c,a
    ld b,#0
    ld hl,(string_destination)
    ld a,(string_prefix)
    ld e,a
    ld d,#0
    inc de
    add hl,de
    ex de,hl
    ld hl,(string_source)
    inc hl
    ldir
    ret
.equ string_source, 0xfd6a
.equ string_destination, 0xfd6c
.equ string_position, 0xfd6e
.equ string_count, 0xfd70
.equ string_drop, 0xfd72
.equ string_capacity, 0xfd74
.equ string_prefix, 0xfd75
.equ string_inserted, 0xfd76
.equ string_tail, 0xfd77
.equ string_remaining, 0xfd78

move_bytes:
    ld a,b
    or c
    ret z
    push hl
    or a
    sbc hl,de
    pop hl
    jr nc,move_bytes_forward
    add hl,bc
    dec hl
    ex de,hl
    add hl,bc
    dec hl
    ex de,hl
    lddr
    ret
move_bytes_forward:
    ldir
    ret

set_zero:
    ld b,#32
set_zero_loop:
    ld (hl),#0
    inc hl
    djnz set_zero_loop
    ret
set_load:
    ld (set_source),hl
    ld (set_first),a
    ld a,c
    ld (set_width),a
    push de
    ex de,hl
    call set_zero
    pop hl
    ld a,(set_first)
    ld e,a
    ld d,#0
    add hl,de
    ex de,hl
    ld hl,(set_source)
    ld a,(set_width)
    ld c,a
    ld b,#0
    ldir
    ld hl,#2
    add hl,sp
    ret
set_store:
    ld b,#0
    push de
    ld e,a
    ld d,#0
    add hl,de
    pop de
    ldir
    ret
set_add:
    ld (set_destination),bc
    ld a,e
    cp l
    ret c
    sub l
    inc a
    ld b,a                    ; zero denotes all 256 elements
    ld a,l
    ld (set_element),a
set_add_loop:
    push bc
    ld a,(set_element)
    call set_mask
    ld a,(set_element)
    srl a
    srl a
    srl a
    ld e,a
    ld d,#0
    ld hl,(set_destination)
    add hl,de
    ld a,(hl)
    or c
    ld (hl),a
    ld hl,#set_element
    inc (hl)
    pop bc
    djnz set_add_loop
    ret
set_binary:
    ld (set_mode),a
    ld (set_drop),bc
    pop bc
    ld (argument_return),bc
    ld b,#32
set_binary_loop:
    ld a,(set_mode)
    cp #1
    jr z,set_intersect
    cp #2
    jr z,set_difference
    ld a,(de)
    or (hl)
    jr set_binary_byte
set_intersect:
    ld a,(de)
    and (hl)
    jr set_binary_byte
set_difference:
    ld a,(hl)
    cpl
    ld c,a
    ld a,(de)
    and c
set_binary_byte:
    ld (hl),a
    inc hl
    inc de
    djnz set_binary_loop
    ld hl,#0
    add hl,sp
    ld (argument_source),hl
    ld de,(set_drop)
    ld hl,#0xffe0
    add hl,de
    add hl,sp
    ld sp,hl
    ex de,hl
    ld hl,(argument_source)
    ld bc,#32
    jp move_string_tail
set_compare:
    ld (set_mode),a
    ld b,#32
    ld c,#0
set_compare_loop:
    ld a,(set_mode)
    cp #2
    jr z,set_subset
    cp #3
    jr z,set_superset
    ld a,(de)
    xor (hl)
    jr set_compare_byte
set_subset:
    ld a,(hl)
    cpl
    ld (set_byte),a
    ld a,(de)
    jr set_compare_mask
set_superset:
    ld a,(de)
    cpl
    ld (set_byte),a
    ld a,(hl)
set_compare_mask:
    push hl
    ld hl,#set_byte
    and (hl)
    pop hl
set_compare_byte:
    or c
    ld c,a
    inc hl
    inc de
    djnz set_compare_loop
    ld hl,#0
    ld a,c
    or a
    jr nz,set_compare_result
    inc hl
set_compare_result:
    ld a,(set_mode)
    cp #1
    ret nz
    ld a,l
    xor #1
    ld l,a
    ret
set_member:
    ld a,d
    or a
    jr nz,set_member_false
    ld a,e
    call set_mask
    ld a,e
    srl a
    srl a
    srl a
    ld e,a
    ld d,#0
    add hl,de
    ld a,(hl)
    and c
    ld hl,#0
    ret z
    inc hl
    ret
set_member_false:
    ld hl,#0
    ret
.equ set_source, 0xfe85
.equ set_destination, 0xfe87
.equ set_drop, 0xfe89
.equ set_first, 0xfe8b
.equ set_width, 0xfe8c
.equ set_element, 0xfe8d
.equ set_mode, 0xfe8e
.equ set_byte, 0xfe8f
; A element -> C mask, B zero. HL/DE are retained by both set consumers.
set_mask:
    and #7
    ld b,a
    ld c,#1
    ret z
set_mask_loop:
    sla c
    djnz set_mask_loop
    ret

; Managed stack/heap checks preserve both alternate and index register sets.
; The CALL return points at two inline words: reservation bytes and source PC.
checked_guard:
    push af
    push bc
    push de
    push hl
    ld hl,#10
    add hl,sp
    ld (guard_sp),hl
    ld hl,#8
    add hl,sp
    ld e,(hl)
    inc hl
    ld d,(hl)
    ex de,hl
    ld e,(hl)
    inc hl
    ld d,(hl)
    inc hl
    ld (guard_bytes),de
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    ld (0x40be),bc
    ex de,hl
    ld hl,#8
    add hl,sp
    ld (hl),e
    inc hl
    ld (hl),d
    ld hl,(guard_sp)
    ld de,(runtime_stack_budget)
    or a
    sbc hl,de
    jp c,heap_error
    ld de,(guard_bytes)
    or a
    sbc hl,de
    jp c,heap_error
    call check_boundary
    pop hl
    pop de
    pop bc
    pop af
    ret

; HL is prospective SP minus all requested bytes and emergency headroom.
; Preserve HL/DE/BC for dynamic String helpers. No callbacks or syscalls occur.
check_boundary:
    ld a,h
    cp #0xc8
    jp c,heap_error
    push de
    push bc
    push hl
    ld hl,(heap_frontier)
    ld de,(heap_floor)
    or a
    sbc hl,de
    jp c,heap_error
    pop hl
    ld de,(heap_frontier)
    or a
    sbc hl,de
    jp c,heap_error
    add hl,de
    ld (guard_sp),hl           ; private prospective CPU boundary
    push hl
    ld hl,(recursion_limit)
    or a                       ; DE is still the live heap frontier
    sbc hl,de
    jp c,heap_error
    pop hl
    ld de,(recursion_ceiling)   ; preceding accepted subtraction cleared carry
    sbc hl,de
    jp c,heap_error
    add hl,de
    pop bc
    pop de
    ret
; Heap operations use the lower of the public recursion pointer and this
; invocation's checked CPU boundary. Neither check overwrites RecurPtr.
heap_limit:
    push hl
    ld hl,(recursion_limit)
    ld de,(guard_sp)
    or a
    sbc hl,de
    jr nc,heap_limit_cpu
    add hl,de
    ex de,hl
heap_limit_cpu:
    pop hl
    ret
; Two-byte headers contain an even payload span and a high-bit free flag.
heap_round:
    bit 7,h
    jp nz,heap_error
    bit 0,l
    ret z
    inc hl
    bit 7,h
    jp nz,heap_error
    ret

; Scan one block at HL, after testing HL != frontier. Every header/span is
; validated before following it. Scratch is image-local and non-reentrant.
heap_read:
    ld (heap_at),hl
    ld de,(heap_floor)
    or a
    sbc hl,de
    jp c,heap_error
    ld hl,(heap_frontier)
    ld de,(heap_at)
    or a
    sbc hl,de
    jp c,heap_error
    ld de,#2
    or a
    sbc hl,de
    jp c,heap_error
    ld hl,(heap_at)
    ld c,(hl)
    inc hl
    ld a,(hl)
    and #0x80
    ld (heap_flag),a
    ld a,(hl)
    and #0x7f
    ld b,a
    bit 0,c
    jp nz,heap_error
    ld (heap_span),bc
    inc hl
    add hl,bc
    jp c,heap_error
    ld (heap_next),hl
    ld de,(heap_frontier)
    or a
    sbc hl,de
    jr nc,heap_read_end
    ret
heap_read_end:
    ld a,h
    or l
    jp nz,heap_error
    ret

heap_allocate:
    call heap_round
    ld (heap_want),hl
    ld hl,(heap_floor)
heap_allocate_scan:
    ld de,(heap_frontier)
    or a
    sbc hl,de
    jp z,heap_append
    add hl,de
    call heap_read
    ld a,(heap_flag)
    or a
    jr z,heap_allocate_next
    ld hl,(heap_span)
    ld de,(heap_want)
    or a
    sbc hl,de
    jr c,heap_allocate_next
    ld a,h
    or l
    jr z,heap_allocate_here
    ; An even remainder >=2 can hold its own header (zero payload is valid).
    dec hl
    dec hl
    ld b,h
    ld c,l
    ld hl,(heap_at)
    inc hl
    inc hl
    add hl,de
    ld (hl),c
    inc hl
    ld a,b
    or #0x80
    ld (hl),a
heap_allocate_here:
    ld hl,(heap_at)
heap_allocate_commit:
    ld de,(heap_want)
    ld (hl),e
    inc hl
    ld (hl),d
    inc hl
    ret
heap_allocate_next:
    ld hl,(heap_next)
    jr heap_allocate_scan
heap_append:
    ld hl,(heap_frontier)
    ld (heap_at),hl
    ld de,(heap_want)
    inc de
    inc de
    add hl,de
    jp c,heap_error
    call heap_limit
    or a
    sbc hl,de
    jr c,heap_append_ready
    jp nz,heap_error
heap_append_ready:
    add hl,de
    ld (heap_frontier),hl
    ld hl,(heap_at)
    jr heap_allocate_commit

heap_free:
    ld (heap_target),hl
    ex de,hl
    call heap_round
    ld (heap_want),hl
    ld hl,(heap_floor)
heap_free_scan:
    ld de,(heap_frontier)
    or a
    sbc hl,de
    jp z,heap_error
    add hl,de
    call heap_read
    ld hl,(heap_at)
    inc hl
    inc hl
    ld de,(heap_target)
    or a
    sbc hl,de
    jr z,heap_free_found
    ld hl,(heap_next)
    jr heap_free_scan
heap_free_found:
    ld a,(heap_flag)
    or a
    jp nz,heap_error
    ld hl,(heap_span)
    ld de,(heap_want)
    or a
    sbc hl,de
    jp nz,heap_error
    ld hl,(heap_at)
    inc hl
    set 7,(hl)
    ld hl,#0
    ld (heap_previous),hl
    ld hl,(heap_floor)
heap_coalesce:
    ld de,(heap_frontier)
    or a
    sbc hl,de
    ret z
    add hl,de
    call heap_read
    ld a,(heap_flag)
    or a
    jr nz,heap_coalesce_free
    ld hl,#0
    ld (heap_previous),hl
    jr heap_coalesce_next
heap_coalesce_free:
    ld hl,(heap_previous)
    ld a,h
    or l
    jr z,heap_coalesce_first
    ld c,(hl)
    inc hl
    ld a,(hl)
    and #0x7f
    ld b,a
    ld hl,(heap_span)
    inc hl
    inc hl
    add hl,bc
    ld b,h
    ld c,l
    ld hl,(heap_previous)
    ld (heap_at),hl
    ld (hl),c
    inc hl
    ld a,b
    or #0x80
    ld (hl),a
    jr heap_coalesce_tail
heap_coalesce_first:
    ld hl,(heap_at)
    ld (heap_previous),hl
heap_coalesce_tail:
    ld hl,(heap_next)
    ld de,(heap_frontier)
    or a
    sbc hl,de
    jr nz,heap_coalesce_next
    ld hl,(heap_at)
    ld (heap_frontier),hl
    ret
heap_coalesce_next:
    ld hl,(heap_next)
    jr heap_coalesce

heap_release:
    ld (heap_target),hl
    ld hl,(heap_floor)
heap_release_scan:
    ld de,(heap_target)
    or a
    sbc hl,de
    jr z,heap_release_ready
    add hl,de
    ld de,(heap_frontier)
    or a
    sbc hl,de
    jp z,heap_error
    add hl,de
    call heap_read
    ld hl,(heap_next)
    jr heap_release_scan
heap_release_ready:
    ld hl,(heap_target)
    ld (heap_frontier),hl
    ret

heap_availability:
    ld (heap_mode),a
    cp #2
    ld hl,(heap_frontier)
    ret z
    ld hl,#0
    ld (heap_total),hl
    ld (heap_largest),hl
    ld hl,(heap_floor)
heap_availability_scan:
    ld de,(heap_frontier)
    or a
    sbc hl,de
    jr z,heap_availability_tail
    add hl,de
    call heap_read
    ld a,(heap_flag)
    or a
    jr z,heap_availability_next
    ld hl,(heap_span)
    call heap_available_add
heap_availability_next:
    ld hl,(heap_next)
    jr heap_availability_scan
heap_availability_tail:
    call heap_limit
    ex de,hl
    ld de,(heap_frontier)
    or a
    sbc hl,de
    jp c,heap_error
    ld de,#2
    or a
    sbc hl,de
    jr c,heap_availability_result
    res 0,l
    call heap_available_add
heap_availability_result:
    ld a,(heap_mode)
    or a
    ld hl,(heap_total)
    ret z
    ld hl,(heap_largest)
    ret
heap_available_add:
    push hl
    ld de,(heap_total)
    add hl,de
    ld (heap_total),hl
    pop hl
    ld de,(heap_largest)
    or a
    sbc hl,de
    ret c
    add hl,de
    ld (heap_largest),hl
    ret

stack_set:
    ld a,h
    cp #0xc8
    jp c,heap_error
    ld (heap_target),hl
    pop bc
    ld (heap_return),bc
    ld de,#0xfc00
    or a
    sbc hl,de
    jr c,stack_set_end
    jp nz,heap_error
stack_set_end:
    ld hl,(0x401e)
    inc hl
    ld de,(heap_target)
    or a
    sbc hl,de
    jp c,heap_error
    ld hl,(heap_target)
    ld de,(runtime_stack_budget)
    or a
    sbc hl,de
    jp c,heap_error
    ld de,(heap_frontier)
    or a
    sbc hl,de
    jp c,heap_error
    add hl,de
    ld (guard_sp),hl
    ld de,(recursion_ceiling)
    sbc hl,de
    jp c,heap_error
    ld hl,(heap_target)
    ld sp,hl
    ld bc,(heap_return)
    push bc
    ret

.equ guard_sp, 0xfd60
.equ guard_bytes, 0xfd62
.equ heap_want, 0xfd66
.equ heap_at, 0xfd68
.equ heap_next, 0xfd6a
.equ heap_span, 0xfd6c
.equ heap_target, 0xfd6e
.equ heap_previous, 0xfd70
.equ heap_total, 0xfd72
.equ heap_largest, 0xfd74
.equ heap_return, 0xfd76
.equ heap_flag, 0xfd78
.equ heap_mode, 0xfd79

; Zero count never becomes a 65536-byte write. Caller owns the raw span.
fill_bytes:
    ld d,a
    ld a,b
    or c
    ret z
fill_bytes_loop:
    ld (hl),d
    inc hl
    dec bc
    ld a,b
    or c
    jr nz,fill_bytes_loop
    ret
; Runtime input is session state, separate from persistent Pascal data.
input_reset:
    ld hl,#input_state_begin
    ld de,#input_state_begin+1
    ld bc,#input_reset_count
    ld (hl),#0
    ldir
    ld a,#255
    ld (echo_fd),a
    ret
runtime_prefix_end::
.area _CODE
heap_error:
    ld hl,(0x40be)
    jp stack_error
; Terminal cleanup releases native handles without an implicit Pascal Close.
file_cleanup::
    ld c,a
    ld a,(0xfe74)
    or a
    jr nz,file_cleanup_owned
    ld a,(0xfe75)
    or a
    ret z
file_cleanup_owned:
    ld a,c
    push bc                    ; checking policy and operation
    ld bc,#0
    push bc                    ; native detail
    push bc                    ; error
    ld l,a
    ld h,#0
    push hl                    ; operation
    ld hl,#0
    add hl,sp                  ; six-byte caller-owned cleanup job
    ld a,#2                    ; terminal cleanup releases module backing
    call 0xfc80
    pop bc                     ; operation
    pop de                     ; error word
    pop hl                     ; native detail
    pop bc                     ; checking policy
    ld a,d
    or e
    jr nz,file_cleanup_error
    ld a,(0xfe7f)
    or a
    ret z
    ld l,a
    ld h,#0
    ld de,#0x1ff               ; owned module page could not be released
file_cleanup_error:
    ld a,b
    cp #2
    jr nz,file_cleanup_report
    ld a,l
    ld (0xfe73),a              ; secondary native detail, never the primary fault
    ret
file_cleanup_report:
    ld a,d
    or a
    jr nz,file_cleanup_runtime
    ld a,(io_status)
    or a
    jr nz,file_cleanup_pending
    ld (0xfe7a),hl
    ld a,e
    ld (io_status),a
file_cleanup_pending:
    ld a,b
    or a
    ret z
    ld a,(io_status)
    ld b,#3
    jp call_error
file_cleanup_runtime:
    ld a,e
    ld b,#2
    jp call_error
