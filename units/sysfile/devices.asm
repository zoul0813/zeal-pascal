; Devices routines for the compiler-private SYSFILE Unit.
; Maintained assembly migrated from the verified backend; the C implementation
; remains the legacy/host behavior oracle, not a production build input.

fragment f_serial_stream
; Function serial_stream
_serial_stream:
    reference r522, f_serial_stream, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	ld	hl, #-5
	add	hl, sp
	ld	sp, hl
; uint8_t writing=job->operation==ZF_TEXT_WRITE||job->operation==ZF_TEXT_WRITELN;
    reference r523, f_serial_stream, 1, sf_data, _job, 0
	ld	hl, (_job)
	ex	(sp), hl
	pop	hl
	push	hl
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	sub	a, #0x15
	or	a, b
	ld	a, #0x01
	jr	Z, f_serial_stream_L00278
	xor	a, a
f_serial_stream_L00278:
	ld	-3 (ix), a
	bit	0, -3 (ix)
	jr	NZ, f_serial_stream_L00135
	ld	a, c
	sub	a, #0x17
	or	a, b
	jr	Z, f_serial_stream_L00135
	xor	a, a
	jr	f_serial_stream_L00136
f_serial_stream_L00135:
	ld	a, #0x01
f_serial_stream_L00136:
	ld	-2 (ix), a
; uint8_t error,restore=0;
	ld	-1 (ix), #0x00
; if(job->operation==ZF_TEXT_BEGIN&&!(job->count&2)) return;
	ld	a, c
	sub	a, #0x1c
	or	a, b
	jr	NZ, f_serial_stream_L00102
	pop	hl
	push	hl
	ld	de, #0x0009
	add	hl, de
	dec	hl
	bit	1, (hl)
    reference r524, f_serial_stream, 1, f_serial_stream, f_serial_stream_L00132, 0
	jp	Z,f_serial_stream_L00132
f_serial_stream_L00102:
; if(job->operation==ZF_TEXT_WRITE&&!USER(job->data)[0]) return;
	ld	a, -3 (ix)
	or	a, a
	jr	Z, f_serial_stream_L00105
	pop	bc
	push	bc
	ld	hl, #6
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, (bc)
	or	a, a
    reference r525, f_serial_stream, 1, f_serial_stream, f_serial_stream_L00132, 0
	jp	Z,f_serial_stream_L00132
f_serial_stream_L00105:
; native_fd=-1;
    reference r526, f_serial_stream, 1, sf_data, _native_fd, 0
	ld	hl, #_native_fd
	ld	(hl), #0xff
; if(writing) {
	ld	a, -2 (ix)
	or	a, a
	jr	Z, f_serial_stream_L00114
; if(!serial_open()) { native_fd=-1; return; }
    reference r527, f_serial_stream, 1, f_serial_open, f_serial_open, 0
	call	_serial_open
	or	a, a
	jr	NZ, f_serial_stream_L00108
    reference r528, f_serial_stream, 1, sf_data, _native_fd, 0
	ld	hl, #_native_fd
	ld	(hl), #0xff
    reference r529, f_serial_stream, 1, f_serial_stream, f_serial_stream_L00132, 0
	jp	f_serial_stream_L00132
f_serial_stream_L00108:
; fd=(int8_t)native_fd;
    reference r530, f_serial_stream, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
	ld	c, a
; error=ioctl(fd,SERIAL_CMD_GET_ATTR,&serial_attribute);
	push	bc
    reference r531, f_serial_stream, 1, sf_data, _serial_attribute, 0
	ld	hl, #_serial_attribute
	push	hl
	ld	l, #0x80
	ld	a, c
    reference r532, f_serial_stream, 1, f_ioctl, f_ioctl, 0
	call	_ioctl
	pop	bc
; if(!error) {
	or	a, a
	jr	NZ, f_serial_stream_L00110
; restore=1;
	ld	-1 (ix), #0x01
; error=ioctl(fd,SERIAL_CMD_SET_ATTR,(void *)(uintptr_t)(serial_attribute|SERIAL_ATTR_MODE_RAW));
    reference r533, f_serial_stream, 1, sf_data, _serial_attribute, 0
	ld	a, (_serial_attribute+0)
	set	0, a
	ld	d, #0x00
	ld	e, a
	push	de
	ld	l, #0x81
	ld	a, c
    reference r534, f_serial_stream, 1, f_ioctl, f_ioctl, 0
	call	_ioctl
f_serial_stream_L00110:
; if(error) fail(0x20,error);
	or	a, a
	jr	Z, f_serial_stream_L00114
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r535, f_serial_stream, 1, f_fail, f_fail, 0
	call	_fail
f_serial_stream_L00114:
; uint8_t writing=job->operation==ZF_TEXT_WRITE||job->operation==ZF_TEXT_WRITELN;
    reference r536, f_serial_stream, 2, sf_data, _job, 0
	ld	bc, (_job)
; if(!job->error) switch(job->operation) {
	ld	e, c
	ld	d, b
	ld	hl, #22
	add	hl, de
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	NZ, f_serial_stream_L00123
; uint8_t writing=job->operation==ZF_TEXT_WRITE||job->operation==ZF_TEXT_WRITELN;
	ld	l, c
	ld	h, b
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
; if(!job->error) switch(job->operation) {
	ld	c, e
	ld	b, d
	ld	a, c
	sub	a, #0x14
	or	a, b
	jr	Z, f_serial_stream_L00116
; uint8_t writing=job->operation==ZF_TEXT_WRITE||job->operation==ZF_TEXT_WRITELN;
	ld	a, e
	sub	a, #0x17
	or	a, d
	ld	a, #0x01
	jr	Z, f_serial_stream_L00286
	xor	a, a
f_serial_stream_L00286:
	ld	c, a
; if(!job->error) switch(job->operation) {
	ld	a, e
	sub	a, #0x15
	or	a, d
	jr	Z, f_serial_stream_L00119
	ld	a, e
	sub	a, #0x16
	or	a, d
	jr	Z, f_serial_stream_L00117
	ld	a, c
	or	a, a
	jr	NZ, f_serial_stream_L00119
	ld	a, e
	sub	a, #0x1c
	or	a, d
	jr	NZ, f_serial_stream_L00120
; case ZF_TEXT_BEGIN: text_peek(); break;
    reference r537, f_serial_stream, 1, f_text_peek, f_text_peek, 0
	call	_text_peek
	jr	f_serial_stream_L00123
; case ZF_TEXT_READ: text_read(); break;
f_serial_stream_L00116:
    reference r538, f_serial_stream, 1, f_text_read, f_text_read, 0
	call	_text_read
	jr	f_serial_stream_L00123
; case ZF_TEXT_READLN: text_readln(); break;
f_serial_stream_L00117:
    reference r539, f_serial_stream, 1, f_text_readln, f_text_readln, 0
	call	_text_readln
	jr	f_serial_stream_L00123
; case ZF_TEXT_WRITE: case ZF_TEXT_WRITELN:
f_serial_stream_L00119:
; text_write(job->operation==ZF_TEXT_WRITELN); break;
	ld	a, c
    reference r540, f_serial_stream, 1, f_text_write, f_text_write, 0
	call	_text_write
	jr	f_serial_stream_L00123
; default: text_predicate(job->operation); break;
f_serial_stream_L00120:
	ex	de, hl
    reference r541, f_serial_stream, 1, f_text_predicate, f_text_predicate, 0
	call	_text_predicate
; }
f_serial_stream_L00123:
; if(restore) {
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_serial_stream_L00127
; fd=(int8_t)native_fd;
    reference r542, f_serial_stream, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
	ld	c, a
; error=ioctl(fd,SERIAL_CMD_SET_ATTR,(void *)(uintptr_t)serial_attribute);
    reference r543, f_serial_stream, 1, sf_data, _serial_attribute, 0
	ld	a, (_serial_attribute+0)
	ld	d, #0x00
	ld	e, a
	push	de
	ld	l, #0x81
	ld	a, c
    reference r544, f_serial_stream, 1, f_ioctl, f_ioctl, 0
	call	_ioctl
; if(error) fail(0xf0,error);
	or	a, a
	jr	Z, f_serial_stream_L00127
	push	af
	inc	sp
	ld	hl, #0x00f0
    reference r545, f_serial_stream, 1, f_fail, f_fail, 0
	call	_fail
f_serial_stream_L00127:
; if(native_fd>=0) {
    reference r546, f_serial_stream, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
	bit	7, a
	jr	NZ, f_serial_stream_L00131
; error=close_file((uint8_t)native_fd);
    reference r547, f_serial_stream, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r548, f_serial_stream, 1, f_close_file, f_close_file, 0
	call	_close_file
; if(error) fail(0xf0,error);
	or	a, a
	jr	Z, f_serial_stream_L00131
	push	af
	inc	sp
	ld	hl, #0x00f0
    reference r549, f_serial_stream, 1, f_fail, f_fail, 0
	call	_fail
f_serial_stream_L00131:
; native_fd=-1;
    reference r550, f_serial_stream, 1, sf_data, _native_fd, 0
	ld	hl, #_native_fd
	ld	(hl), #0xff
f_serial_stream_L00132:
; }
	ld	sp, ix
	pop	ix
	ret
; static void logical_stream(void) {
endfragment f_serial_stream

fragment f_logical_stream
; Function logical_stream
_logical_stream:
    reference r551, f_logical_stream, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	ld	hl, #-6
	add	hl, sp
	ld	sp, hl
; switch(job->operation) {
    reference r552, f_logical_stream, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	dec	a
	or	a, b
	jr	Z, f_logical_stream_L00102
	ld	a, c
	sub	a, #0x02
	or	a, b
	jr	Z, f_logical_stream_L00102
	ld	a, c
	sub	a, #0x03
	or	a, b
    reference r553, f_logical_stream, 1, f_logical_stream, f_logical_stream_L00137, 0
	jp	Z,f_logical_stream_L00137
	ld	a, c
	sub	a, #0x09
	or	a, b
	jr	Z, f_logical_stream_L00109
	ld	e, c
	ld	d, b
	ld	a, e
	sub	a, #0x14
	or	a, d
	jr	Z, f_logical_stream_L00112
	ld	a, c
	sub	a, #0x15
	or	a, b
	jr	Z, f_logical_stream_L00117
	ld	a, c
	sub	a, #0x16
	or	a, b
	jr	Z, f_logical_stream_L00109
	ld	a, c
	sub	a, #0x17
	or	a, b
	jr	Z, f_logical_stream_L00117
	ld	a, c
	sub	a, #0x18
	or	a, b
	jr	Z, f_logical_stream_L00109
	ld	a, c
	sub	a, #0x19
	or	a, b
	jr	Z, f_logical_stream_L00109
	ld	a, c
	sub	a, #0x1a
	or	a, b
	jr	Z, f_logical_stream_L00109
	ld	a, c
	sub	a, #0x1c
	or	a, b
	jr	Z, f_logical_stream_L00109
	jr	f_logical_stream_L00122
; case ZF_RESET: case ZF_REWRITE:
f_logical_stream_L00102:
; record->state=device_access(record->device);
    reference r554, f_logical_stream, 2, sf_data, _record, 0
	ld	bc, (_record)
	ld	hl, #0x0010
	add	hl, bc
	ex	de, hl
	ld	hl, #26
	add	hl, bc
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	push	de
    reference r555, f_logical_stream, 1, f_device_access, f_device_access, 0
	call	_device_access
	pop	de
	ld	c, a
	ld	b, #0x00
	ld	a, c
	ld	(de), a
	inc	de
	ld	a, b
	ld	(de), a
; record->next=0;  return;
    reference r556, f_logical_stream, 1, sf_data, _record, 0
	ld	hl, (_record)
	inc	hl
	ld	(hl), #0x00
; case ZF_CLOSE: return;
    reference r557, f_logical_stream, 1, f_logical_stream, f_logical_stream_L00137, 0
	jp	f_logical_stream_L00137
; case ZF_EOLN: case ZF_SEEKEOF: case ZF_SEEKEOLN:
f_logical_stream_L00109:
; if(!access(0)) return;
	xor	a, a
    reference r558, f_logical_stream, 1, f_access, f_access, 0
	call	_access
	or	a, a
	jr	NZ, f_logical_stream_L00123
    reference r559, f_logical_stream, 1, f_logical_stream, f_logical_stream_L00137, 0
	jp	f_logical_stream_L00137
; case ZF_TEXT_READ:
f_logical_stream_L00112:
; if(!access(0)||!text_input_target()) return;
	xor	a, a
    reference r560, f_logical_stream, 1, f_access, f_access, 0
	call	_access
	or	a, a
    reference r561, f_logical_stream, 1, f_logical_stream, f_logical_stream_L00137, 0
	jp	Z,f_logical_stream_L00137
    reference r562, f_logical_stream, 1, f_text_input_target, f_text_input_target, 0
	call	_text_input_target
	or	a, a
	jr	NZ, f_logical_stream_L00123
    reference r563, f_logical_stream, 1, f_logical_stream, f_logical_stream_L00137, 0
	jp	f_logical_stream_L00137
; case ZF_TEXT_WRITE: case ZF_TEXT_WRITELN:
f_logical_stream_L00117:
; if(!access(1)||(job->operation==ZF_TEXT_WRITE&&!text_output_value())) return;
	ld	a, #0x01
    reference r564, f_logical_stream, 1, f_access, f_access, 0
	call	_access
	or	a, a
    reference r565, f_logical_stream, 1, f_logical_stream, f_logical_stream_L00137, 0
	jp	Z,f_logical_stream_L00137
    reference r566, f_logical_stream, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	sub	a, #0x15
	or	a, b
	jr	NZ, f_logical_stream_L00123
    reference r567, f_logical_stream, 1, f_text_output_value, f_text_output_value, 0
	call	_text_output_value
	or	a, a
	jr	NZ, f_logical_stream_L00123
    reference r568, f_logical_stream, 1, f_logical_stream, f_logical_stream_L00137, 0
	jp	f_logical_stream_L00137
; default: fail(0x20,0); return;
f_logical_stream_L00122:
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r569, f_logical_stream, 1, f_fail, f_fail, 0
	call	_fail
    reference r570, f_logical_stream, 1, f_logical_stream, f_logical_stream_L00137, 0
	jp	f_logical_stream_L00137
; }
f_logical_stream_L00123:
; switch(job->operation) {
    reference r571, f_logical_stream, 2, sf_data, _job, 0
	ld	bc, (_job)
; if((job->operation==ZF_TEXT_WRITE||job->operation==ZF_TEXT_WRITELN)&&
	ld	l, c
	ld	h, b
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
; record->state=device_access(record->device);
    reference r572, f_logical_stream, 2, sf_data, _record, 0
	ld	iy, (_record)
; job->stream=record->device; return;
	ld	hl, #0x0022
	add	hl, bc
	ex	(sp), hl
; ((record->device==ZF_STANDARD_LST&&(job->count&ZF_OUTPUT_LST))||
	ld	a, 26 (iy)
	ld	-4 (ix), a
	ld	a, 27 (iy)
	ld	-3 (ix), a
	ld	a, -4 (ix)
	sub	a, #0x06
	or	a, -3 (ix)
	ld	a, #0x01
	jr	Z, f_logical_stream_L00324
	xor	a, a
f_logical_stream_L00324:
	ld	-2 (ix), a
; (record->device==ZF_STANDARD_AUX&&(job->count&ZF_OUTPUT_AUX)))) {
	ld	a, -4 (ix)
	sub	a, #0x07
	or	a, -3 (ix)
	ld	a, #0x01
	jr	Z, f_logical_stream_L00326
	xor	a, a
f_logical_stream_L00326:
	ld	-1 (ix), a
; if((job->operation==ZF_TEXT_WRITE||job->operation==ZF_TEXT_WRITELN)&&
	ld	a, e
	sub	a, #0x15
	or	a, d
	jr	Z, f_logical_stream_L00127
	ld	a, e
	sub	a, #0x17
	or	a, d
	jr	NZ, f_logical_stream_L00125
f_logical_stream_L00127:
; !(job->count&ZF_OUTPUT_NATIVE)&&(job->count&ZF_OUTPUT_OVERRIDES)&&
	ld	hl, #8
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	bit	7, b
	jr	NZ, f_logical_stream_L00125
	bit	6, b
	jr	Z, f_logical_stream_L00125
; ((record->device==ZF_STANDARD_LST&&(job->count&ZF_OUTPUT_LST))||
	ld	a, -2 (ix)
	or	a, a
	jr	Z, f_logical_stream_L00131
	bit	0, c
	jr	NZ, f_logical_stream_L00124
f_logical_stream_L00131:
; (record->device==ZF_STANDARD_AUX&&(job->count&ZF_OUTPUT_AUX)))) {
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_logical_stream_L00125
	bit	1, c
	jr	Z, f_logical_stream_L00125
f_logical_stream_L00124:
; job->stream=record->device; return;
	pop	hl
	push	hl
	ld	a, -4 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -3 (ix)
	ld	(hl), a
	jr	f_logical_stream_L00137
f_logical_stream_L00125:
; if(record->device==ZF_STANDARD_LST) { fail(0x20,ERR_NOT_SUPPORTED); return; }
	ld	a, -2 (ix)
	or	a, a
	jr	Z, f_logical_stream_L00134
	ld	a, #0x03
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r573, f_logical_stream, 1, f_fail, f_fail, 0
	call	_fail
	jr	f_logical_stream_L00137
f_logical_stream_L00134:
; if(record->device==ZF_STANDARD_AUX) { serial_stream();  return; }
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_logical_stream_L00136
    reference r574, f_logical_stream, 1, f_serial_stream, f_serial_stream, 0
	call	_serial_stream
	jr	f_logical_stream_L00137
f_logical_stream_L00136:
; job->stream=record->device;
	pop	hl
	push	hl
	ld	a, -4 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -3 (ix)
	ld	(hl), a
; job->lookahead=(uint8_t)record->next|((record->state&LOOKAHEAD)?0x100u:0);
    reference r575, f_logical_stream, 2, sf_data, _job, 0
	ld	bc, (_job)
	ld	hl, #0x0024
	add	hl, bc
	ex	de, hl
; record->state=device_access(record->device);
    reference r576, f_logical_stream, 2, sf_data, _record, 0
	ld	bc, (_record)
; job->lookahead=(uint8_t)record->next|((record->state&LOOKAHEAD)?0x100u:0);
	ld	l, c
	ld	h, b
	inc	hl
	ld	a, (hl)
	push	bc
	pop	iy
	push	af
	ld	l, 16 (iy)
	pop	af
	bit	4, l
	jr	Z, f_logical_stream_L00139
	ld	-2 (ix), #0x00
	ld	-1 (ix), #0x01
	jr	f_logical_stream_L00140
f_logical_stream_L00139:
	ld	-2 (ix), #0x00
	ld	-1 (ix), #0x00
f_logical_stream_L00140:
	ld	c, a
	ld	b, -1 (ix)
	ld	a, c
	ld	(de), a
	inc	de
	ld	a, b
	ld	(de), a
f_logical_stream_L00137:
; }
	ld	sp, ix
	pop	ix
	ret
; void zf_op_reset(void) {activate(0);}
endfragment f_logical_stream
