; Validation routines for the compiler-private SYSFILE Unit.
; Maintained assembly migrated from the verified backend; the C implementation
; remains the legacy/host behavior oracle, not a production build input.

fragment f_within

; HL=address, DE=size; callee-popped stack words low, high. A=boolean.
; Compare by subtraction so wraparound cannot authorize a transfer.
_within:
    pop iy
    pop bc
    ld a,l
    sub c
    ld a,h
    sbc a,b
    jr c,within_low_fail
    pop bc
    ld a,c
    sub l
    ld l,a
    ld a,b
    sbc a,h
    ld h,a
    jr c,within_fail
    or a
    sbc hl,de
    jr c,within_fail
    ld a,#1
    jp (iy)
within_low_fail:
    pop bc
within_fail:
    xor a
    jp (iy)
endfragment f_within

fragment f_heap_span
; Function heap_span
_heap_span:
	push	ix
	ld	ix,#0
	add	ix,sp
	ld	iy, #-8
	add	iy, sp
	ld	sp, iy
	ld	-2 (ix), l
	ld	-1 (ix), h
	ld	-4 (ix), e
	ld	-3 (ix), d
; uint16_t cursor=job->heap_floor,span,next,found=0,header;
    reference r17, f_heap_span, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x000e
	add	hl, de
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	hl, #0x0000
	ex	(sp), hl
; while(cursor<job->heap_frontier) {
f_heap_span_L00109:
; uint16_t cursor=job->heap_floor,span,next,found=0,header;
    reference r18, f_heap_span, 1, sf_data, _job, 0
	ld	hl, (_job)
; while(cursor<job->heap_frontier) {
	ld	de, #0x0010
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, c
	sub	a, e
	ld	a, b
	sbc	a, d
	jr	NC, f_heap_span_L00111
; if(job->heap_frontier-cursor<2) return 0;
	ld	a, e
	sub	a, c
	ld	e, a
	ld	a, d
	sbc	a, b
	ld	d, a
	ld	a, e
	sub	a, #0x02
	ld	a, d
	sbc	a, #0x00
	jr	NC, f_heap_span_L00102
	xor	a, a
	jr	f_heap_span_L00115
f_heap_span_L00102:
; header=word(cursor); span=header&0x7fff;
	push	bc
	ld	l, c
	ld	h, b
    reference r19, f_heap_span, 1, f_word, f_word, 0
	call	_word
	push	de
	pop	iy
	pop	bc
	push	iy
	pop	de
	res	7, d
; if(span&1||span>job->heap_frontier-cursor-2) return 0;
	bit	0, e
	jr	NZ, f_heap_span_L00103
    reference r20, f_heap_span, 1, sf_data, _job, 0
	ld	hl, (_job)
	push	bc
	ld	bc, #0x0010
	add	hl, bc
	pop	bc
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	cp	a, a
	sbc	hl, bc
	dec	hl
	dec	hl
	ld	-6 (ix), e
	ld	-5 (ix), d
	ld	a, l
	sub	a, -6 (ix)
	ld	a, h
	sbc	a, -5 (ix)
	jr	NC, f_heap_span_L00104
f_heap_span_L00103:
	xor	a, a
	jr	f_heap_span_L00115
f_heap_span_L00104:
; next=cursor+2+span;
	ld	l, c
	ld	h, b
	inc	hl
	inc	hl
	ld	a, e
	add	a, l
	ld	c, a
	ld	a, d
	adc	a, h
	ld	b, a
; if(!(header&0x8000)&&within(at,size,cursor+2,next)) found=cursor+2;
	add	iy, iy
	jr	C, f_heap_span_L00109
	push	hl
	push	bc
	push	bc
	push	hl
	ld	e, -4 (ix)
	ld	d, -3 (ix)
	ld	l, -2 (ix)
	ld	h, -1 (ix)
    reference r21, f_heap_span, 1, f_within, f_within, 0
	call	_within
	pop	bc
	pop	hl
	or	a, a
    reference r22, f_heap_span, 1, f_heap_span, f_heap_span_L00109, 0
	jp	Z,f_heap_span_L00109
	ex	(sp), hl
; cursor=next;
    reference r23, f_heap_span, 1, f_heap_span, f_heap_span_L00109, 0
	jp	f_heap_span_L00109
f_heap_span_L00111:
; if(cursor!=job->heap_frontier||!found) return 0;
	ex	de,hl
	cp	a, a
	sbc	hl, bc
	jr	NZ, f_heap_span_L00112
	ld	a, -7 (ix)
	or	a, -8 (ix)
	jr	NZ, f_heap_span_L00113
f_heap_span_L00112:
	xor	a, a
	jr	f_heap_span_L00115
f_heap_span_L00113:
; return 1;
	ld	a, #0x01
f_heap_span_L00115:
; }
	ld	sp, ix
	pop	ix
	ret
; static uint8_t readable(uint16_t at,uint16_t size) {
endfragment f_heap_span

fragment f_readable

; HL=address, DE=size. A=boolean. IX preserved; other registers scratch.
; Resolve the program's current execution-window aliases before validating.
_readable:
    push ix
    reference readable_job, f_readable, 2, sf_data, _job, 0
    ld ix,(_job)
    ld a,(#0x4012)
    bit 2,a
    jr z,readable_two_pages
    ld a,h
    cp #0x40
    jr c,readable_fail
    and #0x3f
    or #0x40
    ld h,a
    reference readable_static1, f_readable, 1, f_readable, readable_static, 0
    call readable_static
    or a
    jr nz,readable_done
    set 7,h
    jr readable_stack
readable_two_pages:
    bit 1,a
    jr z,readable_normal
    bit 7,h
    jr z,readable_normal
    res 6,h
    reference readable_static2, f_readable, 1, f_readable, readable_static, 0
    call readable_static
    or a
    jr nz,readable_done
    set 6,h
    jr readable_stack
readable_normal:
    reference readable_static3, f_readable, 1, f_readable, readable_static, 0
    call readable_static
    or a
    jr nz,readable_done
readable_stack:
    ld a,l
    sub 28(ix)
    ld a,h
    sbc a,29(ix)
    jr c,readable_heap
    push hl
    add hl,de
    jr c,readable_stack_bad
    ld a,#0
    sub l
    ld a,#0xfc
    sbc a,h
readable_stack_bad:
    pop hl
    jr nc,readable_yes
readable_heap:
    pop ix
    reference readable_heap_call, f_readable, 1, f_heap_span, _heap_span, 0
    jp _heap_span
readable_fail:
    xor a
    jr readable_done
readable_yes:
    ld a,#1
readable_done:
    pop ix
    ret
; Check [HL,HL+DE] against [program.data,job.static_end], preserving HL/DE.
readable_static:
    ld bc,(#0x4018)
    ld a,l
    sub c
    ld a,h
    sbc a,b
    jr c,readable_static_no
    push hl
    add hl,de
    jr c,readable_static_overflow
    ld a,18(ix)
    sub l
    ld a,19(ix)
    sbc a,h
readable_static_overflow:
    pop hl
    jr c,readable_static_no
    ld a,#1
    ret
readable_static_no:
    xor a
    ret
endfragment f_readable

fragment f_descriptor

; HL=family output, DE=width output. Validate before following descriptor.
_descriptor:
    push hl
    push de
    push ix
    reference descriptor_job, f_descriptor, 2, sf_data, _job, 0
    ld ix,(_job)
    ld l,4(ix)
    ld h,5(ix)
    ld a,l
    sub 30(ix)
    ld a,h
    sbc a,31(ix)
    jr c,descriptor_bad
    ld bc,#3
    add hl,bc
    jr c,descriptor_bad
    ld a,18(ix)
    sub l
    ld a,19(ix)
    sbc a,h
    jr c,descriptor_bad
    dec hl
    ld b,(hl)
    dec hl
    ld c,(hl)
    dec hl
    ld a,(hl)
    pop ix
    pop de
    ex de,hl
    ld (hl),c
    inc hl
    ld (hl),b
    pop hl
    ld (hl),a
    inc hl
    ld (hl),#0
    cp #3
    jr nc,descriptor_no
    cp #1
    jr z,descriptor_yes
    or a
    jr nz,descriptor_block
    ld a,b
    or c
    jr z,descriptor_yes
    jr descriptor_no
 descriptor_block:
    ld a,c
    xor #128
    or b
    jr nz,descriptor_no
 descriptor_yes:
    ld a,#1
    ret
 descriptor_bad:
    pop ix
    pop de
    pop hl
 descriptor_no:
    xor a
    ret
endfragment f_descriptor

fragment f_initialize
; Function initialize
_initialize:
	ld	c, l
	ld	b, h
; memset(record,0,sizeof(*record));
    reference r40, f_initialize, 1, sf_data, _record, 0
	ld	hl, (_record)
	push	bc
	ld	b, #0xb0
f_initialize_L00103:
	ld	(hl), #0x00
	inc	hl
	djnz	f_initialize_L00103
	pop	bc
; record->marker=FILE_MARKER;record->family=family;record->width=width;
    reference r41, f_initialize, 1, sf_data, _record, 0
	ld	hl, (_record)
	push	de
	ld	de, #0x000c
	add	hl, de
	pop	de
	ld	(hl), #0x5a
	inc	hl
	ld	(hl), #0x46
    reference r42, f_initialize, 1, sf_data, _record, 0
	ld	hl, (_record)
	push	de
	ld	de, #0x000e
	add	hl, de
	pop	de
	ld	(hl), c
	inc	hl
	ld	(hl), b
    reference r43, f_initialize, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0006
	add	hl, bc
	ld	(hl), e
	inc	hl
	ld	(hl), d
; record->fd=0xffffu;
    reference r44, f_initialize, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0012
	add	hl, bc
	ld	(hl), #0xff
	inc	hl
	ld	(hl), #0xff
; }
	ret
; static uint8_t bound(uint16_t family,uint16_t width) {
endfragment f_initialize

fragment f_bound

; HL=family, DE=width. Marker and complete bounded NUL path are mandatory.
_bound:
    push ix
    reference bound_record, f_bound, 2, sf_data, _record, 0
    ld ix,(_record)
    ld a,12(ix)
    cp #0x5a
    jr nz,bound_no
    ld a,13(ix)
    cp #0x46
    jr nz,bound_no
    ld a,14(ix)
    cp l
    jr nz,bound_no
    ld a,15(ix)
    cp h
    jr nz,bound_no
    ld a,6(ix)
    cp e
    jr nz,bound_no
    ld a,7(ix)
    cp d
    jr nz,bound_no
    push ix
    pop hl
    ld bc,#48
    add hl,bc
    ld bc,#128
    xor a
    cpir
    jr nz,bound_no
    ld a,#1
    pop ix
    ret
bound_no:
    xor a
    pop ix
    ret
endfragment f_bound

fragment f_flags

; Reflect Pascal FIB flags from validated internal state; no stack locals.
_flags:
    reference flags_record, f_flags, 2, sf_data, _record, 0
    ld iy,(_record)
    ld a,26(iy)
    ld c,a
    or 27(iy)
    jr z,flags_type_ready
    ld a,#1
    ld b,27(iy)
    inc b
    dec b
    jr nz,flags_type_ready
    ld a,c
    cp #3
    ld a,#2
    jr z,flags_type_ready
    ld a,c
    cp #6
    ld a,#3
    jr z,flags_type_ready
    ld a,c
    cp #7
    ld a,#4
    jr z,flags_type_ready
    ld a,c
    cp #8
    ld a,#5
    jr z,flags_type_ready
    ld a,#1
flags_type_ready:
    ld c,a
    ld b,16(iy)
    bit 1,b
    jr z,flags_not_readable
    set 7,c
flags_not_readable:
    bit 2,b
    jr z,flags_not_writable
    set 6,c
flags_not_writable:
    ld a,14(iy)
    or 15(iy)
    jr z,flags_text
    bit 3,b
    jr z,flags_family
    set 5,c
    jr flags_family
flags_text:
    bit 4,b
    jr z,flags_family
    set 5,c
flags_family:
    ld a,14(iy)
    dec a
    or 15(iy)
    jr nz,flags_store
    set 4,c
flags_store:
    ld 0(iy),c
    ret
endfragment f_flags

fragment f_device_access
; Function device_access
_device_access:
; return identity==ZF_STANDARD_KBD?READABLE:identity==ZF_STANDARD_LST?WRITABLE:READABLE|WRITABLE;
	ld	a, l
	sub	a, #0x03
	or	a, h
	jr	NZ, f_device_access_L00103
	ld	a, #0x02
	ret
f_device_access_L00103:
	ld	a, l
	sub	a, #0x06
	or	a, h
	ld	a, #0x04
	ret	Z
	ld	a, #0x06
; }
	ret
; static uint8_t standards(void) {
endfragment f_device_access

fragment f_standards
; Function standards
_standards:
    reference r49, f_standards, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	ld	hl, #-11
	add	hl, sp
	ld	sp, hl
; uint16_t table=job->standard_table,count,i,j,at,object,identity,saved=job->object;
    reference r50, f_standards, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	-5 (ix), l
	ld	-4 (ix), h
	ld	bc,#32
	add	hl,bc
	ld	a, (hl)
	ld	-3 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-2 (ix), a
	ld	l, -5 (ix)
	ld	h, -4 (ix)
	inc	hl
	inc	hl
	ld	a, (hl)
	ld	-11 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-10 (ix), a
; uint8_t identities=0;
	ld	-1 (ix), #0x00
; if(!table)return 1;
	ld	a, -2 (ix)
	or	a, -3 (ix)
	jr	NZ, f_standards_L00102
	ld	a, #0x01
    reference r51, f_standards, 1, f_standards, f_standards_L00131, 0
	jp	f_standards_L00131
f_standards_L00102:
; if(!within(table,2,job->code_origin,job->static_end))goto invalid;
	ld	a, -5 (ix)
	ld	-7 (ix), a
	ld	a, -4 (ix)
	ld	-6 (ix), a
	ld	l, -7 (ix)
	ld	h, -6 (ix)
	ld	de, #0x0012
	add	hl, de
	ld	a, (hl)
	ld	-7 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-6 (ix), a
	ld	l, -5 (ix)
	ld	h, -4 (ix)
	ld	de, #0x001e
	add	hl, de
	ld	a, (hl)
	ld	-5 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-4 (ix), a
	ld	l, -7 (ix)
	ld	h, -6 (ix)
	push	hl
	ld	l, -5 (ix)
	ld	h, -4 (ix)
	push	hl
	ld	de, #0x0002
	ld	l, -3 (ix)
	ld	h, -2 (ix)
    reference r52, f_standards, 1, f_within, f_within, 0
	call	_within
	or	a, a
    reference r53, f_standards, 1, f_standards, f_standards_L00122, 0
	jp	Z, f_standards_L00122
; count=word(table);
	ld	l, -3 (ix)
	ld	h, -2 (ix)
    reference r54, f_standards, 1, f_word, f_word, 0
	call	_word
	ld	-9 (ix), e
	ld	-8 (ix), d
; if(count>ZF_STANDARD_USR||!within(table+2,count*4,job->code_origin,job->static_end))goto invalid;
	ld	a, #0x08
	cp	a, -9 (ix)
	ld	a, #0x00
	sbc	a, -8 (ix)
    reference r55, f_standards, 1, f_standards, f_standards_L00122, 0
	jp	C, f_standards_L00122
; uint16_t table=job->standard_table,count,i,j,at,object,identity,saved=job->object;
    reference r56, f_standards, 2, sf_data, _job, 0
	ld	bc, (_job)
; if(count>ZF_STANDARD_USR||!within(table+2,count*4,job->code_origin,job->static_end))goto invalid;
	ld	e, c
	ld	d, b
	ld	hl, #18
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	hl, #30
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	l, -9 (ix)
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	push	hl
	pop	iy
	ld	l, -3 (ix)
	ld	h, -2 (ix)
	inc	hl
	inc	hl
	ld	-7 (ix), l
	ld	-6 (ix), h
	push	de
	push	bc
	push	iy
	pop	de
    reference r57, f_standards, 1, f_within, f_within, 0
	call	_within
	or	a, a
    reference r58, f_standards, 1, f_standards, f_standards_L00122, 0
	jp	Z, f_standards_L00122
; for(i=0;i<count;++i) {
	ld	bc, #0x0000
f_standards_L00127:
	ld	a, c
	sub	a, -9 (ix)
	ld	a, b
	sbc	a, -8 (ix)
    reference r59, f_standards, 1, f_standards, f_standards_L00151, 0
	jp	NC, f_standards_L00151
; at=table+2+4*i;object=word(at);identity=word(at+2);
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	ld	e, -7 (ix)
	ld	d, -6 (ix)
	add	hl, de
	push	hl
	push	bc
    reference r60, f_standards, 1, f_word, f_word, 0
	call	_word
	ld	-5 (ix), e
	ld	-4 (ix), d
	pop	bc
	pop	hl
	inc	hl
	inc	hl
	push	bc
    reference r61, f_standards, 1, f_word, f_word, 0
	call	_word
	ld	-3 (ix), e
	ld	-2 (ix), d
	pop	bc
; if(!identity||identity>ZF_STANDARD_USR||(identities&(1u<<(identity-1)))||
	ld	a, -2 (ix)
	or	a, -3 (ix)
    reference r62, f_standards, 1, f_standards, f_standards_L00122, 0
	jp	Z, f_standards_L00122
	ld	a, #0x08
	cp	a, -3 (ix)
	ld	a, #0x00
	sbc	a, -2 (ix)
    reference r63, f_standards, 1, f_standards, f_standards_L00122, 0
	jp	C, f_standards_L00122
	ld	e, -3 (ix)
	dec	e
	ld	hl, #0x0001
	inc	e
	jr	f_standards_L00256
f_standards_L00255:
	add	hl, hl
f_standards_L00256:
	dec	e
	jr	NZ,f_standards_L00255
	ld	e, -1 (ix)
	ld	d, #0x00
	ld	a, l
	and	a, e
	ld	e, a
	ld	a, h
	and	a, d
	or	a, e
    reference r64, f_standards, 1, f_standards, f_standards_L00122, 0
	jp	NZ, f_standards_L00122
; job->code_origin))goto invalid;
    reference r65, f_standards, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 30 (iy)
	ld	h, 31 (iy)
; word(0x4018),
	push	hl
	push	bc
	ld	hl, #0x4018
    reference r66, f_standards, 1, f_word, f_word, 0
	call	_word
	pop	bc
	pop	hl
; !within(object,ZF_FILE_BYTES,
	push	bc
	push	hl
	push	de
	ld	de, #0x00b0
	ld	l, -5 (ix)
	ld	h, -4 (ix)
    reference r67, f_standards, 1, f_within, f_within, 0
	call	_within
	pop	bc
	or	a, a
    reference r68, f_standards, 1, f_standards, f_standards_L00122, 0
	jp	Z, f_standards_L00122
; identities|=1u<<(identity-1);
	ld	e, -3 (ix)
	dec	e
	ld	a, #0x01
	inc	e
	jr	f_standards_L00258
f_standards_L00257:
	add	a, a
f_standards_L00258:
	dec	e
	jr	NZ,f_standards_L00257
	or	a, -1 (ix)
	ld	-1 (ix), a
; for(j=0;j<i;++j)if(word(table+2+4*j)==object)goto invalid;
	ld	de, #0x0000
f_standards_L00124:
	ld	a, e
	sub	a, c
	ld	a, d
	sbc	a, b
	jr	NC, f_standards_L00128
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, hl
	ld	a, l
	add	a, -7 (ix)
	ld	l, a
	ld	a, h
	adc	a, -6 (ix)
	ld	h, a
	push	bc
	push	de
    reference r69, f_standards, 1, f_word, f_word, 0
	call	_word
	ex	de, hl
	pop	de
	pop	bc
	ld	a, l
	sub	a, -5 (ix)
	jr	NZ, f_standards_L00259
	ld	a, h
	sub	a, -4 (ix)
    reference r70, f_standards, 1, f_standards, f_standards_L00122, 0
	jp	Z,f_standards_L00122
f_standards_L00259:
	inc	de
	jr	f_standards_L00124
f_standards_L00128:
; for(i=0;i<count;++i) {
	inc	bc
    reference r71, f_standards, 1, f_standards, f_standards_L00127, 0
	jp	f_standards_L00127
; for(i=0;i<count;++i) {
f_standards_L00151:
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
f_standards_L00130:
	ld	a, -2 (ix)
	sub	a, -9 (ix)
	ld	a, -1 (ix)
	sbc	a, -8 (ix)
    reference r72, f_standards, 1, f_standards, f_standards_L00121, 0
	jp	NC, f_standards_L00121
; at=table+2+4*i;object=word(at);record=(Record *)USER(object);identity=word(at+2);
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	add	hl, hl
	add	hl, hl
	ld	e, -7 (ix)
	ld	d, -6 (ix)
	add	hl, de
	push	hl
    reference r73, f_standards, 1, f_word, f_word, 0
	call	_word
	pop	hl
	inc	hl
	inc	hl
    reference r74, f_standards, 2, sf_data, _record, 0
	ld	(_record), de
    reference r75, f_standards, 1, f_word, f_word, 0
	call	_word
; if(bound(0,0)&&record->standard==identity)continue;
	push	de
	ld	de, #0x0000
	ld	h, d
	ld	l, e
    reference r76, f_standards, 1, f_bound, f_bound, 0
	call	_bound
	pop	de
	or	a, a
	jr	Z, f_standards_L00118
    reference r77, f_standards, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0018
	add	hl, bc
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	cp	a, a
	sbc	hl, de
	jr	Z, f_standards_L00120
f_standards_L00118:
; initialize(0,0);record->standard=identity;
	push	de
	ld	de, #0x0000
	ld	h, d
	ld	l, e
    reference r78, f_standards, 1, f_initialize, f_initialize, 0
	call	_initialize
	pop	de
    reference r79, f_standards, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0018
	add	hl, bc
	ld	(hl), e
	inc	hl
	ld	(hl), d
; record->device=identity>=ZF_STANDARD_KBD?identity:0;
    reference r80, f_standards, 2, sf_data, _record, 0
	ld	bc, (_record)
	ld	hl, #0x001a
	add	hl, bc
	ld	-4 (ix), l
	ld	-3 (ix), h
	ld	a, e
	sub	a, #0x03
	ld	a, d
	sbc	a, #0x00
	jr	C, f_standards_L00133
	ld	c, e
	ld	b, d
	jr	f_standards_L00134
f_standards_L00133:
	ld	bc, #0x0000
f_standards_L00134:
	ld	l, -4 (ix)
	ld	h, -3 (ix)
	ld	(hl), c
	inc	hl
	ld	(hl), b
; record->state=device_access(identity);flags();
    reference r81, f_standards, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0010
	add	hl, bc
	ex	de, hl
	push	de
    reference r82, f_standards, 1, f_device_access, f_device_access, 0
	call	_device_access
	pop	hl
	ld	c, #0x00
	ld	(hl), a
	inc	hl
	ld	(hl), c
    reference r83, f_standards, 1, f_flags, f_flags, 0
	call	_flags
f_standards_L00120:
; for(i=0;i<count;++i) {
	inc	-2 (ix)
    reference r84, f_standards, 1, f_standards, f_standards_L00130, 0
	jp	NZ,f_standards_L00130
	inc	-1 (ix)
    reference r85, f_standards, 1, f_standards, f_standards_L00130, 0
	jp	f_standards_L00130
f_standards_L00121:
; record=(Record *)USER(saved);return 1;
	pop	hl
	push	hl
    reference r86, f_standards, 1, sf_data, _record, 0
	ld	(_record), hl
	ld	a, #0x01
	jr	f_standards_L00131
; invalid:
f_standards_L00122:
; fail(0x1ff,0);return 0;
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x01ff
    reference r87, f_standards, 1, f_fail, f_fail, 0
	call	_fail
	xor	a, a
f_standards_L00131:
; }
	ld	sp, ix
	pop	ix
	ret
; static uint8_t standard_identity(void) {
endfragment f_standards

fragment f_standard_identity
; Function standard_identity
_standard_identity:
    reference r88, f_standard_identity, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	ld	hl, #-8
	add	hl, sp
	ld	sp, hl
; uint16_t table=job->standard_table,count,i;
    reference r89, f_standard_identity, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0020
	add	hl, de
	ld	a, (hl)
	ld	-8 (ix), a
	inc	hl
	ld	a, (hl)
; if(!table)return 0;
	ld	-7 (ix), a
	or	a, -8 (ix)
	jr	NZ, f_standard_identity_L00102
	xor	a, a
	jr	f_standard_identity_L00109
f_standard_identity_L00102:
; count=word(table);
	pop	hl
	push	hl
    reference r90, f_standard_identity, 1, f_word, f_word, 0
	call	_word
	ld	-6 (ix), e
	ld	-5 (ix), d
; for(i=0;i<count;++i)if(word(table+2+4*i)==job->object)return (uint8_t)word(table+4+4*i);
	ld	bc, #0x0000
f_standard_identity_L00107:
	ld	a, c
	sub	a, -6 (ix)
	ld	a, b
	sbc	a, -5 (ix)
	jr	NC, f_standard_identity_L00105
	ld	a, -8 (ix)
	ld	-4 (ix), a
	ld	a, -7 (ix)
	ld	-3 (ix), a
	ld	l, -4 (ix)
	ld	h, -3 (ix)
	inc	hl
	inc	hl
	ld	e, c
	ld	d, b
	ex	de, hl
	add	hl, hl
	add	hl, hl
	ex	de, hl
	add	hl, de
	push	bc
	push	de
    reference r91, f_standard_identity, 1, f_word, f_word, 0
	call	_word
	ld	-2 (ix), e
	ld	-1 (ix), d
	pop	de
	pop	bc
    reference r92, f_standard_identity, 1, sf_data, _job, 0
	ld	hl, (_job)
	inc	hl
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	sub	a, -2 (ix)
	jr	NZ, f_standard_identity_L00108
	ld	a, h
	sub	a, -1 (ix)
	jr	NZ, f_standard_identity_L00108
	ld	l, -4 (ix)
	ld	h, -3 (ix)
	ld	bc, #0x0004
	add	hl, bc
	add	hl, de
    reference r93, f_standard_identity, 1, f_word, f_word, 0
	call	_word
	ld	a, e
	jr	f_standard_identity_L00109
f_standard_identity_L00108:
	inc	bc
	jr	f_standard_identity_L00107
f_standard_identity_L00105:
; return 0;
	xor	a, a
f_standard_identity_L00109:
; }
	ld	sp, ix
	pop	ix
	ret
; static uint8_t bookkeeping_status(uint32_t size,uint32_t position) {
endfragment f_standard_identity
