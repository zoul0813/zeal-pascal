; Text routines for the compiler-private SYSFILE Unit.
; Maintained assembly migrated from the verified backend; the C implementation
; remains the legacy/host behavior oracle, not a production build input.

fragment f_text_peek
; Function text_peek
_text_peek:
    reference r205, f_text_peek, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
	push	af
; uint16_t size=1;
	ld	hl, #0x0001
	ex	(sp), hl
; if(!(record->state&LOOKAHEAD)) {
    reference r206, f_text_peek, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	-2 (ix), l
	ld	-1 (ix), h
	pop	de
	pop	hl
	push	hl
	push	de
	ld	de, #0x0011
	add	hl, de
	dec	hl
	bit	4, (hl)
    reference r207, f_text_peek, 1, f_text_peek, f_text_peek_L00117, 0
	jp	NZ,f_text_peek_L00117
; if(record->device&&!serial_open()) return 26;
	pop	de
	pop	hl
	push	hl
	push	de
	ld	de, #0x001b
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	Z, f_text_peek_L00102
    reference r208, f_text_peek, 1, f_serial_open, f_serial_open, 0
	call	_serial_open
	or	a, a
	jr	NZ, f_text_peek_L00102
	ld	a, #0x1a
    reference r209, f_text_peek, 1, f_text_peek, f_text_peek_L00118, 0
	jp	f_text_peek_L00118
f_text_peek_L00102:
; if(!record->device&&!disk_open(0))return 26;
    reference r210, f_text_peek, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x001b
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a,l
	jr	NZ, f_text_peek_L00105
    reference r211, f_text_peek, 1, f_disk_open, f_disk_open, 0
	call	_disk_open
	or	a, a
	jr	NZ, f_text_peek_L00105
	ld	a, #0x1a
    reference r212, f_text_peek, 1, f_text_peek, f_text_peek_L00118, 0
	jp	f_text_peek_L00118
f_text_peek_L00105:
; error=read((int8_t)native_fd,bytes,&size);
	ld	hl, #0
	add	hl, sp
	push	hl
    reference r213, f_text_peek, 1, sf_data, _bytes, 0
	ld	de, #_bytes
    reference r214, f_text_peek, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r215, f_text_peek, 1, f_read, f_read, 0
	call	_read
	ld	c, a
; if(error) { native_error(error,0); return 26; }
	or	a, a
	jr	Z, f_text_peek_L00108
	ld	l, #0x00
	ld	a, c
    reference r216, f_text_peek, 1, f_native_error, f_native_error, 0
	call	_native_error
	ld	a, #0x1a
    reference r217, f_text_peek, 1, f_text_peek, f_text_peek_L00118, 0
	jp	f_text_peek_L00118
f_text_peek_L00108:
; if(size>1) { fail(0x99,ERR_INVALID_PARAMETER); return 26; }
	pop	bc
	push	bc
	ld	a, #0x01
	cp	a, c
	ld	a, #0x00
	sbc	a, b
	jr	NC, f_text_peek_L00110
	ld	a, #0x06
	push	af
	inc	sp
	ld	hl, #0x0099
    reference r218, f_text_peek, 1, f_fail, f_fail, 0
	call	_fail
	ld	a, #0x1a
	jr	f_text_peek_L00118
f_text_peek_L00110:
; if(!(record->state&LOOKAHEAD)) {
    reference r219, f_text_peek, 1, sf_data, _record, 0
	ld	hl, (_record)
; if(!size&&record->device) { fail(0x99,0); return 26; }
	ld	de, #0x001a
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, -3 (ix)
	or	a, -4 (ix)
	jr	NZ, f_text_peek_L00112
	ld	a, d
	or	a, e
	jr	Z, f_text_peek_L00112
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0099
    reference r220, f_text_peek, 1, f_fail, f_fail, 0
	call	_fail
	ld	a, #0x1a
	jr	f_text_peek_L00118
f_text_peek_L00112:
; if(!record->device) {native_position+=size;text_remember();}
	ld	a, d
	or	a, e
	jr	NZ, f_text_peek_L00115
	pop	bc
	push	bc
	ld	de, #0x0000
	ld	a, c
    reference r221, f_text_peek, 1, sf_data, _native_position, 0
	ld	hl, #_native_position
	add	a, (hl)
	ld	(hl), a
	inc	hl
	ld	a, b
	adc	a, (hl)
	ld	(hl), a
	inc	hl
	ld	a, e
	adc	a, (hl)
	ld	(hl), a
	inc	hl
	ld	a, d
	adc	a, (hl)
	ld	(hl), a
    reference r222, f_text_peek, 1, f_text_remember, f_text_remember, 0
	call	_text_remember
f_text_peek_L00115:
; record->next=size?bytes[0]:26;
    reference r223, f_text_peek, 2, sf_data, _record, 0
	ld	de, (_record)
	inc	de
	ld	a, -3 (ix)
	or	a, -4 (ix)
	jr	Z, f_text_peek_L00120
    reference r224, f_text_peek, 1, sf_data, _bytes, 0
	ld	a, (#_bytes + 0)
	jr	f_text_peek_L00121
f_text_peek_L00120:
	ld	a, #0x1a
f_text_peek_L00121:
	ld	(de), a
; if(!(record->state&LOOKAHEAD)) {
    reference r225, f_text_peek, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0010
	add	hl, bc
; record->state|=LOOKAHEAD;
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	dec	hl
	set	4, c
	ld	(hl), c
	inc	hl
	ld	(hl), b
f_text_peek_L00117:
; return (uint8_t)record->next;
    reference r226, f_text_peek, 1, sf_data, _record, 0
	ld	hl, (_record)
	inc	hl
	ld	a, (hl)
f_text_peek_L00118:
; }
	ld	sp, ix
	pop	ix
	ret
; static uint8_t text_take(void) {
endfragment f_text_peek

fragment f_text_take
; Function text_take
_text_take:
; uint8_t c=text_peek();
    reference r227, f_text_take, 1, f_text_peek, f_text_peek, 0
	call	_text_peek
; if(c!=26) record->state&=(uint16_t)~LOOKAHEAD;
	ld	c, a
	sub	a, #0x1a
	jr	Z, f_text_take_L00102
    reference r228, f_text_take, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0010
	add	hl, de
	ld	a, (hl)
	inc	hl
	ld	b, (hl)
	dec	hl
	res	4, a
	ld	(hl), a
	inc	hl
	ld	(hl), b
f_text_take_L00102:
; return c;
	ld	a, c
; }
	ret
; static uint8_t text_space(uint8_t c,uint8_t lines) {
endfragment f_text_take

fragment f_text_space
; Function text_space
_text_space:
; return c==32||c==9||(lines&&(c==13||c==10));
	cp	a, #0x20
	jr	Z, f_text_space_L00104
	cp	a, #0x09
	jr	Z, f_text_space_L00104
	inc	l
	dec	l
	jr	Z, f_text_space_L00103
	cp	a, #0x0d
	jr	Z, f_text_space_L00104
	sub	a, #0x0a
	jr	Z, f_text_space_L00104
f_text_space_L00103:
	xor	a, a
	ret
f_text_space_L00104:
	ld	a, #0x01
; }
	ret
; static void text_predicate(uint16_t operation) {
endfragment f_text_space

fragment f_text_predicate
; Function text_predicate
_text_predicate:
	push	ix
	ld	ix,#0
	add	ix,sp
	push	af
; uint8_t c=text_peek();
	push	hl
    reference r229, f_text_predicate, 1, f_text_peek, f_text_peek, 0
	call	_text_peek
	ld	c, a
	pop	de
; if(operation==ZF_SEEKEOF||operation==ZF_SEEKEOLN)
	ld	a, e
	sub	a, #0x19
	or	a, d
	ld	a, #0x01
	jr	Z, f_text_predicate_L00163
	xor	a, a
f_text_predicate_L00163:
	ld	-2 (ix), a
	ld	a, e
	sub	a, #0x1a
	or	a, d
	ld	a, #0x01
	jr	Z, f_text_predicate_L00165
	xor	a, a
f_text_predicate_L00165:
	ld	-1 (ix), a
	ld	a, -2 (ix)
	or	a, a
	jr	NZ, f_text_predicate_L00102
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_text_predicate_L00106
; while(!job->error&&text_space(c,operation==ZF_SEEKEOF)) {
f_text_predicate_L00102:
    reference r230, f_text_predicate, 1, sf_data, _job, 0
	ld	hl, (_job)
	push	bc
	ld	bc, #0x0017
	add	hl, bc
	pop	bc
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_text_predicate_L00106
	push	bc
	push	de
	ld	l, -2 (ix)
	ld	a, c
    reference r231, f_text_predicate, 1, f_text_space, f_text_space, 0
	call	_text_space
	pop	de
	pop	bc
	or	a, a
	jr	Z, f_text_predicate_L00106
; text_take(); c=text_peek();
	push	de
    reference r232, f_text_predicate, 1, f_text_take, f_text_take, 0
	call	_text_take
    reference r233, f_text_predicate, 1, f_text_peek, f_text_peek, 0
	call	_text_peek
	ld	c, a
	pop	de
	jr	f_text_predicate_L00102
f_text_predicate_L00106:
; job->result=c==26||((operation==ZF_EOLN||operation==ZF_SEEKEOLN)&&c==13);
    reference r234, f_text_predicate, 1, sf_data, _job, 0
	ld	hl, (_job)
	push	de
	ld	de, #0x0014
	add	hl, de
	pop	de
	ld	a, c
	sub	a, #0x1a
	jr	Z, f_text_predicate_L00111
	ld	a, e
	sub	a, #0x18
	or	a, d
	jr	Z, f_text_predicate_L00117
	ld	b, -1 (ix)
	bit	0, b
	jr	Z, f_text_predicate_L00110
f_text_predicate_L00117:
	ld	a, c
	sub	a, #0x0d
	jr	Z, f_text_predicate_L00111
f_text_predicate_L00110:
	ld	c, #0x00
	jr	f_text_predicate_L00112
f_text_predicate_L00111:
	ld	c, #0x01
f_text_predicate_L00112:
	xor	a, a
	ld	(hl), c
	inc	hl
	ld	(hl), a
; }
	pop	af
	pop	ix
	ret
; static uint8_t text_input_target(void) {
endfragment f_text_predicate

fragment f_text_input_target
; Function text_input_target
_text_input_target:
    reference r235, f_text_input_target, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	ld	hl, #-5
	add	hl, sp
	ld	sp, hl
; uint16_t out=job->data;
    reference r236, f_text_input_target, 2, sf_data, _job, 0
	ld	bc, (_job)
	push	bc
	pop	iy
	ld	a, 6 (iy)
	ld	-4 (ix), a
	ld	a, 7 (iy)
	ld	-3 (ix), a
; uint8_t kind=(uint8_t)job->count,limit=(uint8_t)(job->count>>8),valid;
	ld	hl, #8
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	a, (hl)
	ld	-5 (ix), c
	ld	-1 (ix), a
; if(kind!=ZF_TEXT_STRING&&limit) { fail(0x1ff,0); return 0; }
	ld	a, -5 (ix)
	dec	a
	ld	a, #0x01
	jr	Z, f_text_input_target_L00201
	xor	a, a
f_text_input_target_L00201:
	ld	c, a
	bit	0, c
	jr	NZ, f_text_input_target_L00102
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_text_input_target_L00102
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x01ff
    reference r237, f_text_input_target, 1, f_fail, f_fail, 0
	call	_fail
	xor	a, a
    reference r238, f_text_input_target, 1, f_text_input_target, f_text_input_target_L00112, 0
	jp	f_text_input_target_L00112
f_text_input_target_L00102:
; if(kind==ZF_TEXT_STRING) valid=readable(out,(uint16_t)limit+1);
	ld	a, c
	or	a, a
	jr	Z, f_text_input_target_L00108
	ld	e, -1 (ix)
	ld	d, #0x00
	inc	de
	ld	l, -4 (ix)
	ld	h, -3 (ix)
    reference r239, f_text_input_target, 1, f_readable, f_readable, 0
	call	_readable
	ld	-1 (ix), a
    reference r240, f_text_input_target, 1, f_text_input_target, f_text_input_target_L00109, 0
	jp	f_text_input_target_L00109
f_text_input_target_L00108:
; else if(kind==ZF_TEXT_CHAR) valid=readable(out,1);
	ld	a, -5 (ix)
	sub	a, #0x03
	jr	NZ, f_text_input_target_L00105
	ld	de, #0x0001
	ld	l, -4 (ix)
	ld	h, -3 (ix)
    reference r241, f_text_input_target, 1, f_readable, f_readable, 0
	call	_readable
	ld	-1 (ix), a
    reference r242, f_text_input_target, 1, f_text_input_target, f_text_input_target_L00109, 0
	jp	f_text_input_target_L00109
f_text_input_target_L00105:
; else valid=(kind==ZF_TEXT_INTEGER||kind==ZF_TEXT_BYTE||kind==ZF_TEXT_REAL)&&
	ld	a, -5 (ix)
	sub	a, #0x04
	ld	a, #0x01
	jr	Z, f_text_input_target_L00205
	xor	a, a
f_text_input_target_L00205:
	ld	-1 (ix), a
	ld	a, -5 (ix)
	sub	a, #0x05
	ld	a, #0x01
	jr	Z, f_text_input_target_L00207
	xor	a, a
f_text_input_target_L00207:
	ld	-2 (ix), a
	ld	a, -5 (ix)
	or	a, a
	jr	Z, f_text_input_target_L00121
	bit	0, -1 (ix)
	jr	NZ, f_text_input_target_L00121
	bit	0, -2 (ix)
	jr	Z, f_text_input_target_L00114
f_text_input_target_L00121:
; readable(out,ZF_NUMBER_BYTES)&&
	ld	de, #0x0029
	ld	l, -4 (ix)
	ld	h, -3 (ix)
    reference r243, f_text_input_target, 1, f_readable, f_readable, 0
	call	_readable
	ld	-5 (ix), a
	or	a, a
	jr	Z, f_text_input_target_L00114
; readable(word(out+ZF_NUMBER_TARGET),kind==ZF_TEXT_BYTE?1:kind==ZF_TEXT_REAL?6:2);
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_text_input_target_L00126
	ld	-1 (ix), #0x01
	jr	f_text_input_target_L00127
f_text_input_target_L00126:
	ld	a, -2 (ix)
	ld	-1 (ix), a
	or	a, a
	jr	Z, f_text_input_target_L00128
	ld	-1 (ix), #0x06
	jr	f_text_input_target_L00129
f_text_input_target_L00128:
	ld	-1 (ix), #0x02
f_text_input_target_L00129:
f_text_input_target_L00127:
	ld	a, -1 (ix)
	ld	-2 (ix), a
	ld	-1 (ix), #0x00
	ld	l, -4 (ix)
	ld	h, -3 (ix)
    reference r244, f_text_input_target, 1, f_word, f_word, 0
	call	_word
	ld	-4 (ix), e
	ld	-3 (ix), d
	ld	e, -2 (ix)
	ld	d, #0x00
	ld	l, -4 (ix)
	ld	h, -3 (ix)
    reference r245, f_text_input_target, 1, f_readable, f_readable, 0
	call	_readable
	or	a, a
	jr	NZ, f_text_input_target_L00115
f_text_input_target_L00114:
	ld	-1 (ix), #0x00
	jr	f_text_input_target_L00116
f_text_input_target_L00115:
	ld	-1 (ix), #0x01
f_text_input_target_L00116:
f_text_input_target_L00109:
; if(!valid) fail(0x1ff,0);
	ld	a, -1 (ix)
	or	a, a
	jr	NZ, f_text_input_target_L00111
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x01ff
    reference r246, f_text_input_target, 1, f_fail, f_fail, 0
	call	_fail
f_text_input_target_L00111:
; return valid;
	ld	a, -1 (ix)
f_text_input_target_L00112:
; }
	ld	sp, ix
	pop	ix
	ret
; static uint8_t text_output_value(void) {
endfragment f_text_input_target

fragment f_text_output_value
; Function text_output_value
_text_output_value:
; if(!readable(job->data,1)||!readable(job->data,(uint16_t)USER(job->data)[0]+1)) {
    reference r247, f_text_output_value, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0006
	add	hl, de
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	de, #0x0001
    reference r248, f_text_output_value, 1, f_readable, f_readable, 0
	call	_readable
	or	a, a
	jr	Z, f_text_output_value_L00101
    reference r249, f_text_output_value, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0006
	add	hl, de
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	c, l
	ld	b, h
	ld	a, (bc)
	ld	e, a
	ld	d, #0x00
	inc	de
    reference r250, f_text_output_value, 1, f_readable, f_readable, 0
	call	_readable
	or	a, a
	jr	NZ, f_text_output_value_L00102
f_text_output_value_L00101:
; fail(0x1ff,0); return 0;
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x01ff
    reference r251, f_text_output_value, 1, f_fail, f_fail, 0
	call	_fail
	xor	a, a
	ret
f_text_output_value_L00102:
; return 1;
	ld	a, #0x01
; }
	ret
; static void text_read(void) {
endfragment f_text_output_value

fragment f_text_read
; Function text_read
_text_read:
    reference r252, f_text_read, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
	dec	sp
; uint16_t out=job->data;
    reference r253, f_text_read, 2, sf_data, _job, 0
	ld	bc, (_job)
	push	bc
	pop	iy
	ld	a, 6 (iy)
	ld	-3 (ix), a
	ld	a, 7 (iy)
	ld	-2 (ix), a
; uint8_t kind=(uint8_t)job->count,limit=(uint8_t)(job->count>>8),n=0,c;
	ld	hl, #8
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	a, (hl)
	ld	e, c
	ld	-1 (ix), a
; if(!text_input_target()) return;
	push	de
    reference r254, f_text_read, 1, f_text_input_target, f_text_input_target, 0
	call	_text_input_target
	ld	c, a
	pop	de
	ld	a, c
	or	a, a
    reference r255, f_text_read, 1, f_text_read, f_text_read_L00126, 0
	jp	Z,f_text_read_L00126
; if(kind==ZF_TEXT_CHAR) {
	ld	a, e
	sub	a, #0x03
	jr	NZ, f_text_read_L00108
; c=text_take(); if(!job->error) {
    reference r256, f_text_read, 1, f_text_take, f_text_take, 0
	call	_text_take
	ld	-1 (ix), a
    reference r257, f_text_read, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0016
	add	hl, de
	ld	a, (hl)
	inc	hl
	or	a, (hl)
    reference r258, f_text_read, 1, f_text_read, f_text_read_L00126, 0
	jp	NZ,f_text_read_L00126
; if(record->device) record->state&=(uint16_t)~LOOKAHEAD;
    reference r259, f_text_read, 2, sf_data, _record, 0
	ld	bc, (_record)
	ld	e, c
	ld	d, b
	ld	hl, #26
	add	hl, de
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	Z, f_text_read_L00104
	ld	hl, #0x0010
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	dec	hl
	res	4, c
	ld	(hl), c
	inc	hl
	ld	(hl), b
f_text_read_L00104:
; USER(out)[0]=c; job->actual=1;
	pop	bc
	push	bc
	ld	a, -1 (ix)
	ld	(bc), a
    reference r260, f_text_read, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	bc, #0x000a
	add	hl, bc
	ld	(hl), #0x01
	inc	hl
	ld	(hl), #0x00
; return;
    reference r261, f_text_read, 1, f_text_read, f_text_read_L00126, 0
	jp	f_text_read_L00126
f_text_read_L00108:
; if(kind!=ZF_TEXT_STRING) {
	dec	e
	ld	a, #0x01
	jr	Z, f_text_read_L00254
	xor	a, a
f_text_read_L00254:
	ld	c, a
	bit	0, c
	jr	NZ, f_text_read_L00144
; out+=ZF_NUMBER_TOKEN; limit=30;
	pop	de
	push	de
	ld	hl, #0x000a
	add	hl, de
	ex	(sp), hl
	ld	-1 (ix), #0x1e
; c=text_peek();
	push	bc
    reference r262, f_text_read, 1, f_text_peek, f_text_peek, 0
	call	_text_peek
	ld	e, a
	pop	bc
; while(!job->error&&text_space(c,1)) { text_take(); c=text_peek(); }
f_text_read_L00110:
    reference r263, f_text_read, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	a, 22 (iy)
	ld	b, 23 (iy)
	or	a, b
	jr	NZ, f_text_read_L00144
	push	bc
	ld	l, #0x01
	ld	a, e
    reference r264, f_text_read, 1, f_text_space, f_text_space, 0
	call	_text_space
	pop	bc
	or	a, a
	jr	Z, f_text_read_L00144
	push	bc
    reference r265, f_text_read, 1, f_text_take, f_text_take, 0
	call	_text_take
    reference r266, f_text_read, 1, f_text_peek, f_text_peek, 0
	call	_text_peek
	ld	e, a
	pop	bc
	jr	f_text_read_L00110
; while(n<limit&&!job->error) {
f_text_read_L00144:
	ld	b, #0x00
f_text_read_L00121:
	ld	a, b
	sub	a, -1 (ix)
	jr	NC, f_text_read_L00123
    reference r267, f_text_read, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0017
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_text_read_L00123
; c=text_peek();
	push	bc
    reference r268, f_text_read, 1, f_text_peek, f_text_peek, 0
	call	_text_peek
	ld	e, a
	pop	bc
; if(c==26||c==13||(kind!=ZF_TEXT_STRING&&text_space(c,1))) break;
	ld	a,e
	cp	a,#0x1a
	jr	Z, f_text_read_L00123
	sub	a, #0x0d
	jr	Z, f_text_read_L00123
	bit	0, c
	jr	NZ, f_text_read_L00116
	push	bc
	ld	l, #0x01
	ld	a, e
    reference r269, f_text_read, 1, f_text_space, f_text_space, 0
	call	_text_space
	pop	bc
	or	a, a
	jr	NZ, f_text_read_L00123
f_text_read_L00116:
; USER(out+1+n)[0]=text_take(); ++n;
	pop	de
	push	de
	inc	de
	ld	l, b
	ld	h, #0x00
	add	hl, de
	push	hl
	push	bc
    reference r270, f_text_read, 1, f_text_take, f_text_take, 0
	call	_text_take
	pop	bc
	pop	hl
	ld	(hl), a
	inc	b
	jr	f_text_read_L00121
f_text_read_L00123:
; if(!job->error) {
    reference r271, f_text_read, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0017
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_text_read_L00126
; USER(out)[0]=n; job->actual=kind==ZF_TEXT_STRING||n!=0;
	pop	hl
	push	hl
	ld	(hl), b
    reference r272, f_text_read, 2, sf_data, _job, 0
	ld	de, (_job)
	ld	hl, #0x000a
	add	hl, de
	ex	de, hl
	bit	0, c
	jr	NZ, f_text_read_L00129
	ld	a, b
	or	a,a
	jr	NZ, f_text_read_L00129
	ld	c,a
	jr	f_text_read_L00130
f_text_read_L00129:
	ld	c, #0x01
f_text_read_L00130:
	ld	b, #0x00
	ld	a, c
	ld	(de), a
	inc	de
	ld	a, b
	ld	(de), a
f_text_read_L00126:
; }
	ld	sp, ix
	pop	ix
	ret
; static void text_readln(void) {
endfragment f_text_read

fragment f_text_readln
; Function text_readln
_text_readln:
; do { c=text_take(); } while(!job->error&&c!=13&&c!=26);
f_text_readln_L00103:
    reference r273, f_text_readln, 1, f_text_take, f_text_take, 0
	call	_text_take
	ld	b, a
    reference r274, f_text_readln, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0016
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, b
	sub	a, #0x0d
	ld	a, #0x01
	jr	Z, f_text_readln_L00163
	xor	a, a
f_text_readln_L00163:
	ld	c, a
	ld	a, d
	or	a, e
	jr	NZ, f_text_readln_L00105
	bit	0, c
	jr	NZ, f_text_readln_L00105
	ld	a, b
	sub	a, #0x1a
	jr	NZ, f_text_readln_L00103
f_text_readln_L00105:
; if(c==13&&!record->device&&!job->error&&text_peek()==10) text_take();
	ld	a, c
	or	a, a
	ret	Z
    reference r275, f_text_readln, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x001b
	add	hl, bc
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	ret	NZ
	ld	a, d
	or	a, e
	ret	NZ
    reference r276, f_text_readln, 1, f_text_peek, f_text_peek, 0
	call	_text_peek
	sub	a, #0x0a
    reference r277, f_text_readln, 1, f_text_take, f_text_take, 0
	jp	Z,_text_take
; }
	ret
; static void text_write(uint8_t newline) {
endfragment f_text_readln

fragment f_text_write
; Function text_write
_text_write:
	ld	c, a
; if(!record->device&&!disk_open(1))return;
    reference r278, f_text_write, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x001a
	add	hl, de
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	NZ, f_text_write_L00102
	push	bc
	ld	a, #0x01
    reference r279, f_text_write, 1, f_disk_open, f_disk_open, 0
	call	_disk_open
	pop	bc
	or	a, a
	ret	Z
f_text_write_L00102:
; text_position();
	push	bc
    reference r280, f_text_write, 1, f_text_position, f_text_position, 0
	call	_text_position
	pop	bc
; if(newline) { bytes[0]=13; bytes[1]=10; text_bytes(2); }
	ld	a, c
	or	a, a
	jr	Z, f_text_write_L00107
    reference r281, f_text_write, 1, sf_data, _bytes, 0
	ld	hl, #_bytes+0
	ld	(hl), #0x0d
	inc	hl
	ld	(hl), #0x0a
	ld	hl, #0x0002
    reference r282, f_text_write, 1, f_text_bytes, f_text_bytes, 0
	call	_text_bytes
    reference r283, f_text_write, 1, f_text_remember, f_text_remember, 0
	jp	_text_remember
f_text_write_L00107:
; if(!text_output_value()) return;
    reference r284, f_text_write, 1, f_text_output_value, f_text_output_value, 0
	call	_text_output_value
	or	a, a
	ret	Z
; n=USER(job->data)[0];
    reference r285, f_text_write, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 6 (iy)
	ld	h, 7 (iy)
	ld	e, l
	ld	d, h
; transfer(job->data+1,n,1);
	inc	hl
	ld	a, (de)
	ld	e, a
	ld	d, #0x00
	ld	a, #0x01
	push	af
	inc	sp
    reference r286, f_text_write, 1, f_transfer, f_transfer, 0
	call	_transfer
; text_remember();
; }
    reference r287, f_text_write, 1, f_text_remember, f_text_remember, 0
	jp	_text_remember
; static void flush(void) {
endfragment f_text_write

fragment f_flush
; Function flush
_flush:
; if(record->family==1&&(record->state&DIRTY)) {
    reference r288, f_flush, 2, sf_data, _record, 0
	ld	bc, (_record)
	ld	e, c
	ld	d, b
	ld	hl, #14
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, e
	dec	a
	or	a, d
	ret	NZ
	ld	hl, #16
	add	hl, bc
	bit	3, (hl)
	ret	Z
; if(disk_open(1)&&header(1)) record->state&=(uint16_t)~DIRTY;
	ld	a, #0x01
    reference r289, f_flush, 1, f_disk_open, f_disk_open, 0
	call	_disk_open
	or	a, a
	ret	Z
	ld	a, #0x01
    reference r290, f_flush, 1, f_header, f_header, 0
	call	_header
	or	a, a
	ret	Z
; if(record->family==1&&(record->state&DIRTY)) {
    reference r291, f_flush, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0010
	add	hl, bc
; if(disk_open(1)&&header(1)) record->state&=(uint16_t)~DIRTY;
	ld	a, (hl)
	inc	hl
	ld	c, (hl)
	dec	hl
	res	3, a
	ld	(hl), a
	inc	hl
	ld	(hl), c
; }
	ret
; static void close_record(void) {
endfragment f_flush

fragment f_close_record
; Function close_record
_close_record:
    reference r292, f_close_record, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
; if(record->device||!(record->state&OPENED))return;
    reference r293, f_close_record, 1, sf_data, _record, 0
	ld	hl, (_record)
	ex	(sp), hl
	pop	bc
	push	bc
	ld	hl, #26
	add	hl, bc
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	NZ, f_close_record_L00114
	pop	bc
	push	bc
	ld	hl, #16
	add	hl, bc
	ld	c, (hl)
	bit	0, c
	jr	Z, f_close_record_L00114
; if((!record->family&&(record->state&WRITABLE))||
	pop	de
	push	de
	ld	hl, #14
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, d
	or	a, e
	jr	NZ, f_close_record_L00113
	bit	2, c
	jr	NZ, f_close_record_L00109
f_close_record_L00113:
; (record->family==1&&(record->state&DIRTY))) {
	ld	a, e
	dec	a
	or	a, d
	jr	NZ, f_close_record_L00110
	bit	3, c
	jr	Z, f_close_record_L00110
f_close_record_L00109:
; if(!disk_open(1))return;
	ld	a, #0x01
    reference r294, f_close_record, 1, f_disk_open, f_disk_open, 0
	call	_disk_open
	or	a, a
	jr	Z, f_close_record_L00114
; if(!record->family) {bytes[0]=26;text_bytes(1);text_remember();}
    reference r295, f_close_record, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x000e
	add	hl, de
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	NZ, f_close_record_L00107
    reference r296, f_close_record, 1, sf_data, _bytes, 0
	ld	hl, #_bytes+0
	ld	(hl), #0x1a
	ld	hl, #0x0001
    reference r297, f_close_record, 1, f_text_bytes, f_text_bytes, 0
	call	_text_bytes
    reference r298, f_close_record, 1, f_text_remember, f_text_remember, 0
	call	_text_remember
	jr	f_close_record_L00110
f_close_record_L00107:
; else flush();
    reference r299, f_close_record, 1, f_flush, f_flush, 0
	call	_flush
f_close_record_L00110:
; record->state=0;
    reference r300, f_close_record, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0010
	add	hl, bc
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
f_close_record_L00114:
; }
	pop	af
	pop	ix
	ret
; static void activate(uint8_t rewriting) {
endfragment f_close_record
