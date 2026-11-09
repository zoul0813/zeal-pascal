; Dispatch routines for the compiler-private SYSFILE Unit.
; Maintained assembly migrated from the verified backend; the C implementation
; remains the legacy/host behavior oracle, not a production build input.

fragment f_zf_op_reset
; Function zf_op_reset
_zf_op_reset:
	xor	a, a
    reference r577, f_zf_op_reset, 1, f_activate, f_activate, 0
	jp	_activate
; void zf_op_rewrite(void) {activate(1);}
endfragment f_zf_op_reset

fragment f_zf_op_rewrite
; Function zf_op_rewrite
_zf_op_rewrite:
	ld	a, #0x01
    reference r578, f_zf_op_rewrite, 1, f_activate, f_activate, 0
	jp	_activate
; void zf_op_close(void) {close_record();}
endfragment f_zf_op_rewrite

fragment f_zf_op_close
; Function zf_op_close
_zf_op_close:
    reference r579, f_zf_op_close, 1, f_close_record, f_close_record, 0
	jp	_close_record
; void zf_op_read(void) {component(0);}
endfragment f_zf_op_close

fragment f_zf_op_read
; Function zf_op_read
_zf_op_read:
	xor	a, a
    reference r580, f_zf_op_read, 1, f_component, f_component, 0
	jp	_component
; void zf_op_write(void) {component(1);}
endfragment f_zf_op_read

fragment f_zf_op_write
; Function zf_op_write
_zf_op_write:
	ld	a, #0x01
    reference r581, f_zf_op_write, 1, f_component, f_component, 0
	jp	_component
; void zf_op_position(void) {
endfragment f_zf_op_write

fragment f_zf_op_position
; Function zf_op_position
_zf_op_position:
    reference r582, f_zf_op_position, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
	push	af
; if(job->operation==ZF_EOF&&!record->family) {if(access(0))text_predicate(ZF_EOF);return;}
    reference r583, f_zf_op_position, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
    reference r584, f_zf_op_position, 2, sf_data, _record, 0
	ld	iy, (_record)
	ld	a, 14 (iy)
	ld	-2 (ix), a
	ld	a, 15 (iy)
	ld	-1 (ix), a
	ld	a, c
	sub	a, #0x09
	or	a, b
	jr	NZ, f_zf_op_position_L00104
	ld	a, -1 (ix)
	or	a, -2 (ix)
	jr	NZ, f_zf_op_position_L00104
	xor	a, a
    reference r585, f_zf_op_position, 1, f_access, f_access, 0
	call	_access
	or	a, a
	jr	Z, f_zf_op_position_L00111
	ld	hl, #0x0009
    reference r586, f_zf_op_position, 1, f_text_predicate, f_text_predicate, 0
	call	_text_predicate
	jr	f_zf_op_position_L00111
f_zf_op_position_L00104:
; if(!record->family)fail(0x20,0);
	ld	a, -1 (ix)
	or	a, -2 (ix)
	jr	NZ, f_zf_op_position_L00109
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r587, f_zf_op_position, 1, f_fail, f_fail, 0
	call	_fail
	jr	f_zf_op_position_L00111
f_zf_op_position_L00109:
; else if(access(0))job->result=job->operation==ZF_POSITION?record->position:
	xor	a, a
    reference r588, f_zf_op_position, 1, f_access, f_access, 0
	call	_access
	or	a, a
	jr	Z, f_zf_op_position_L00111
; if(job->operation==ZF_EOF&&!record->family) {if(access(0))text_predicate(ZF_EOF);return;}
    reference r589, f_zf_op_position, 2, sf_data, _job, 0
	ld	bc, (_job)
; else if(access(0))job->result=job->operation==ZF_POSITION?record->position:
	ld	hl, #0x0014
	add	hl, bc
	ex	(sp), hl
	ld	a, (bc)
	ld	-2 (ix), a
	inc	bc
	ld	a, (bc)
	ld	-1 (ix), a
; if(job->operation==ZF_EOF&&!record->family) {if(access(0))text_predicate(ZF_EOF);return;}
    reference r590, f_zf_op_position, 2, sf_data, _record, 0
	ld	de, (_record)
; else if(access(0))job->result=job->operation==ZF_POSITION?record->position:
	ld	hl, #0x0008
	add	hl, de
	ld	c, l
	ld	b, h
	ld	a, -2 (ix)
	sub	a, #0x07
	or	a, -1 (ix)
	jr	NZ, f_zf_op_position_L00113
	ld	l, c
	ld	h, b
	ld	c, (hl)
	inc	hl
	ld	d, (hl)
	jr	f_zf_op_position_L00114
f_zf_op_position_L00113:
; job->operation==ZF_SIZE?record->count:record->position>=record->count;
	ld	hl, #4
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, -2 (ix)
	sub	a, #0x08
	or	a, -1 (ix)
	jr	Z, f_zf_op_position_L00116
	ld	l, c
	ld	h, b
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	sub	a, e
	ld	a, b
	sbc	a, d
	ld	a, #0x00
	rla
	xor	a, #0x01
	ld	e, a
	ld	d, #0x00
f_zf_op_position_L00116:
	ld	c, e
f_zf_op_position_L00114:
	pop	hl
	push	hl
	ld	(hl), c
	inc	hl
	ld	(hl), d
f_zf_op_position_L00111:
; }
	ld	sp, ix
	pop	ix
	ret
; void zf_op_seek(void) {
endfragment f_zf_op_position

fragment f_zf_op_seek
; Function zf_op_seek
_zf_op_seek:
; if(!record->family)fail(0x20,0);
    reference r591, f_zf_op_seek, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x000e
	add	hl, de
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	NZ, f_zf_op_seek_L00107
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r592, f_zf_op_seek, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_seek_L00107:
; else if(access(0)) {if(job->count>record->count)fail(0x91,0);else record->position=job->count;}
	xor	a, a
    reference r593, f_zf_op_seek, 1, f_access, f_access, 0
	call	_access
	or	a, a
	ret	Z
    reference r594, f_zf_op_seek, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0008
	add	hl, de
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
; if(!record->family)fail(0x20,0);
    reference r595, f_zf_op_seek, 2, sf_data, _record, 0
	ld	de, (_record)
; else if(access(0)) {if(job->count>record->count)fail(0x91,0);else record->position=job->count;}
	push	de
	pop	iy
	ld	l, 4 (iy)
	ld	h, 5 (iy)
	xor	a, a
	sbc	hl, bc
	jr	NC, f_zf_op_seek_L00102
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0091
    reference r596, f_zf_op_seek, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_seek_L00102:
	ld	hl, #0x0008
	add	hl, de
	ld	(hl), c
	inc	hl
	ld	(hl), b
; }
	ret
; void zf_op_flush(void) {
endfragment f_zf_op_seek

fragment f_zf_op_flush
; Function zf_op_flush
_zf_op_flush:
; if(record->family!=1)fail(0x20,0);else if(record->state&OPENED)flush();else fail(3,0);
    reference r597, f_zf_op_flush, 2, sf_data, _record, 0
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
	jr	Z, f_zf_op_flush_L00105
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r598, f_zf_op_flush, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_flush_L00105:
	ld	hl, #16
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	a, (hl)
	bit	0, c
    reference r599, f_zf_op_flush, 1, f_flush, f_flush, 0
	jp	NZ,_flush
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0003
    reference r600, f_zf_op_flush, 1, f_fail, f_fail, 0
	call	_fail
; }
	ret
; void zf_op_erase(void) {
endfragment f_zf_op_flush

fragment f_zf_op_erase
; Function zf_op_erase
_zf_op_erase:
; if(record->state&OPENED)fail(0x20,0);
    reference r601, f_zf_op_erase, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0011
	add	hl, de
	ld	a, (hl)
	dec	hl
	bit	0, (hl)
	jr	Z, f_zf_op_erase_L00110
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r602, f_zf_op_erase, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_erase_L00110:
; else {error=stat_file();if(error)native_error(error,2);
    reference r603, f_zf_op_erase, 1, f_stat_file, f_stat_file, 0
	call	_stat_file
	ld	c, a
	or	a, a
	jr	Z, f_zf_op_erase_L00107
	ld	l, #0x02
	ld	a, c
    reference r604, f_zf_op_erase, 1, f_native_error, f_native_error, 0
	jp	_native_error
f_zf_op_erase_L00107:
; else if(!D_ISFILE(info.s_flags))fail(0x20,0);
    reference r605, f_zf_op_erase, 1, sf_data, _info, 0
	ld	a, (#_info + 0)
	and	a, #0x01
	dec	a
	jr	Z, f_zf_op_erase_L00104
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r606, f_zf_op_erase, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_erase_L00104:
; else {error=remove_file();if(error)native_error(error,2);}}
    reference r607, f_zf_op_erase, 1, f_remove_file, f_remove_file, 0
	call	_remove_file
	ld	c, a
	or	a, a
	ret	Z
	ld	l, #0x02
	ld	a, c
; }
    reference r608, f_zf_op_erase, 1, f_native_error, f_native_error, 0
	jp	_native_error
; void zf_op_rename(void) {rename_file();}
endfragment f_zf_op_erase

fragment f_zf_op_rename
; Function zf_op_rename
_zf_op_rename:
    reference r609, f_zf_op_rename, 1, f_rename_file, f_rename_file, 0
	jp	_rename_file
; void zf_op_blocks(void) {blocks(job->operation>=ZF_BLOCKWRITE,job->operation&1);}
endfragment f_zf_op_rename

fragment f_zf_op_blocks
; Function zf_op_blocks
_zf_op_blocks:
    reference r610, f_zf_op_blocks, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	and	a, #0x01
	ld	e, a
	ld	a, c
	sub	a, #0x0e
	ld	a, b
	sbc	a, #0x00
	ld	a, #0x00
	rla
	xor	a, #0x01
	ld	c, a
	ld	l, e
	ld	a, c
    reference r611, f_zf_op_blocks, 1, f_blocks, f_blocks, 0
	jp	_blocks
; void zf_op_text_read(void) {if(record->family)fail(0x20,0);else if(access(0))text_read();}
endfragment f_zf_op_blocks

fragment f_zf_op_text_read
; Function zf_op_text_read
_zf_op_text_read:
    reference r612, f_zf_op_text_read, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x000f
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	Z, f_zf_op_text_read_L00104
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r613, f_zf_op_text_read, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_text_read_L00104:
	xor	a, a
    reference r614, f_zf_op_text_read, 1, f_access, f_access, 0
	call	_access
	or	a, a
    reference r615, f_zf_op_text_read, 1, f_text_read, f_text_read, 0
	jp	NZ,_text_read
	ret
; void zf_op_text_readln(void) {if(record->family)fail(0x20,0);else if(access(0))text_readln();}
endfragment f_zf_op_text_read

fragment f_zf_op_text_readln
; Function zf_op_text_readln
_zf_op_text_readln:
    reference r616, f_zf_op_text_readln, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x000f
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	Z, f_zf_op_text_readln_L00104
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r617, f_zf_op_text_readln, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_text_readln_L00104:
	xor	a, a
    reference r618, f_zf_op_text_readln, 1, f_access, f_access, 0
	call	_access
	or	a, a
    reference r619, f_zf_op_text_readln, 1, f_text_readln, f_text_readln, 0
	jp	NZ,_text_readln
	ret
; void zf_op_predicate(void) {if(record->family)fail(0x20,0);else if(access(0))text_predicate(job->operation);}
endfragment f_zf_op_text_readln

fragment f_zf_op_predicate
; Function zf_op_predicate
_zf_op_predicate:
    reference r620, f_zf_op_predicate, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x000e
	add	hl, de
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	Z, f_zf_op_predicate_L00104
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r621, f_zf_op_predicate, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_predicate_L00104:
	xor	a, a
    reference r622, f_zf_op_predicate, 1, f_access, f_access, 0
	call	_access
	or	a, a
	ret	Z
    reference r623, f_zf_op_predicate, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
    reference r624, f_zf_op_predicate, 1, f_text_predicate, f_text_predicate, 0
	jp	_text_predicate
; void zf_op_text_write(void) {if(record->family)fail(0x20,0);else if(access(1))text_write(job->operation==ZF_TEXT_WRITELN);}
endfragment f_zf_op_predicate

fragment f_zf_op_text_write
; Function zf_op_text_write
_zf_op_text_write:
    reference r625, f_zf_op_text_write, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x000e
	add	hl, de
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	Z, f_zf_op_text_write_L00104
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r626, f_zf_op_text_write, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_text_write_L00104:
	ld	a, #0x01
    reference r627, f_zf_op_text_write, 1, f_access, f_access, 0
	call	_access
	or	a, a
	ret	Z
    reference r628, f_zf_op_text_write, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	sub	a, #0x17
	or	a, b
	ld	a, #0x01
	jr	Z, f_zf_op_text_write_L00123
	xor	a, a
f_zf_op_text_write_L00123:
	ld	c, a
    reference r629, f_zf_op_text_write, 1, f_text_write, f_text_write, 0
	jp	_text_write
; void zf_op_begin(void) {if(record->family)fail(0x20,0);}
endfragment f_zf_op_text_write

fragment f_zf_op_begin
; Function zf_op_begin
_zf_op_begin:
    reference r630, f_zf_op_begin, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x000f
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	ret	Z
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r631, f_zf_op_begin, 1, f_fail, f_fail, 0
	call	_fail
	ret
; void zf_op_path(void) {
endfragment f_zf_op_begin

fragment f_zf_op_path
; Function zf_op_path
_zf_op_path:
; uint16_t size=(uint16_t)strlen(record->path)+1;
    reference r632, f_zf_op_path, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0030
	add	hl, bc
    reference r633, f_zf_op_path, 1, f_strlen, f_strlen, 0
	call	_strlen
	ld	c, e
	ld	b, d
	inc	bc
; if(job->count<size||!readable(job->data,size))fail(0x20,0);
    reference r634, f_zf_op_path, 2, sf_data, _job, 0
	ld	de, (_job)
	push	de
	pop	iy
	ld	l, 8 (iy)
	ld	h, 9 (iy)
	xor	a, a
	sbc	hl, bc
	jr	C, f_zf_op_path_L00101
	ld	hl, #6
	add	hl, de
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	push	bc
	ld	e, c
	ld	d, b
    reference r635, f_zf_op_path, 1, f_readable, f_readable, 0
	call	_readable
	pop	bc
	or	a, a
	jr	NZ, f_zf_op_path_L00102
f_zf_op_path_L00101:
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r636, f_zf_op_path, 1, f_fail, f_fail, 0
	call	_fail
	ret
f_zf_op_path_L00102:
; else {memcpy(USER(job->data),record->path,size);job->actual=size-1;}
    reference r637, f_zf_op_path, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0006
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
    reference r638, f_zf_op_path, 1, sf_data, _record, 0
	ld	hl, (_record)
	push	de
	ld	de, #0x0030
	add	hl, de
	pop	de
	push	bc
	ld	a, b
	or	a, c
	jr	Z, f_zf_op_path_L00114
	ldir
f_zf_op_path_L00114:
	pop	bc
    reference r639, f_zf_op_path, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x000a
	add	hl, de
	dec	bc
	ld	(hl), c
	inc	hl
	ld	(hl), b
; }
	ret
; static void dispatch(FileJob *invocation) {
endfragment f_zf_op_path

fragment f_dispatch
; Function dispatch
_dispatch:
	push	ix
	ld	ix,#0
	add	ix,sp
	ld	iy, #-21
	add	iy, sp
	ld	sp, iy
    reference r640, f_dispatch, 1, sf_data, _job, 0
	ld	(_job), hl
; job=invocation;record=(Record *)USER(job->object);
    reference r641, f_dispatch, 2, sf_data, _job, 0
	ld	bc, (_job)
	ld	l, c
	ld	h, b
	inc	hl
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
    reference r642, f_dispatch, 1, sf_data, _record, 0
	ld	(_record), hl
; if(job->operation!=ZF_STREAM_RETURN) {
	ld	l, c
	ld	h, b
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, e
	sub	a, #0x1d
	or	a, d
	jr	Z, f_dispatch_L00102
; job->error=0; job->detail=0; job->result=0; job->actual=0; job->stream=0; job->lookahead=0;
	ld	hl, #0x0016
	add	hl, bc
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
    reference r643, f_dispatch, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	bc, #0x0018
	add	hl, bc
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
    reference r644, f_dispatch, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	bc, #0x0014
	add	hl, bc
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
    reference r645, f_dispatch, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	bc, #0x000a
	add	hl, bc
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
    reference r646, f_dispatch, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	bc, #0x0022
	add	hl, bc
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
    reference r647, f_dispatch, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	bc, #0x0024
	add	hl, bc
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
f_dispatch_L00102:
; if(job->static_end<
    reference r648, f_dispatch, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 18 (iy)
	ld	h, 19 (iy)
; word(0x4018)||word(0x4018)<0x40a0u||
	push	hl
	ld	hl, #0x4018
    reference r649, f_dispatch, 1, f_word, f_word, 0
	call	_word
	pop	hl
	xor	a, a
	sbc	hl, de
    reference r650, f_dispatch, 1, f_dispatch, f_dispatch_L00103, 0
	jp	C, f_dispatch_L00103
	ld	hl, #0x4018
    reference r651, f_dispatch, 1, f_word, f_word, 0
	call	_word
	ld	a, e
	sub	a, #0xa0
	ld	a, d
	sbc	a, #0x40
    reference r652, f_dispatch, 1, f_dispatch, f_dispatch_L00103, 0
	jp	C, f_dispatch_L00103
; job=invocation;record=(Record *)USER(job->object);
    reference r653, f_dispatch, 2, sf_data, _job, 0
	ld	iy, (_job)
; job->static_end>job->heap_floor||
	push	iy
	pop	bc
	ld	hl, #18
	add	hl, bc
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	push	iy
	pop	bc
	ld	hl, #14
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	sub	a, e
	ld	a, b
	sbc	a, d
    reference r654, f_dispatch, 1, f_dispatch, f_dispatch_L00103, 0
	jp	C, f_dispatch_L00103
; job->heap_floor>job->heap_frontier||job->heap_frontier>job->stack_pointer||
	push	iy
	pop	de
	ld	hl, #16
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, e
	sub	a, c
	ld	a, d
	sbc	a, b
	jr	C, f_dispatch_L00103
	push	iy
	pop	bc
	ld	hl, #28
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	sub	a, e
	ld	a, b
	sbc	a, d
	jr	C, f_dispatch_L00103
; job->stack_pointer<0xc800u||job->stack_pointer>0xfc00u||
	ld	a, b
	sub	a, #0xc8
	jr	C, f_dispatch_L00103
	xor	a, a
	cp	a, c
	ld	a, #0xfc
	sbc	a, b
	jr	C, f_dispatch_L00103
; job->code_origin<
	ld	l, 30 (iy)
	ld	h, 31 (iy)
; word(0x4018)||
	push	hl
	ld	hl, #0x4018
    reference r655, f_dispatch, 1, f_word, f_word, 0
	call	_word
	pop	hl
	xor	a, a
	sbc	hl, de
	jr	C, f_dispatch_L00103
; job=invocation;record=(Record *)USER(job->object);
    reference r656, f_dispatch, 2, sf_data, _job, 0
	ld	bc, (_job)
; job->code_origin>job->static_end||
	ld	e, c
	ld	d, b
	ld	hl, #30
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	push	bc
	pop	iy
	ld	l, 18 (iy)
	ld	h, 19 (iy)
	xor	a, a
	sbc	hl, de
	jr	C, f_dispatch_L00103
; (job->heap_floor&1)||!descriptor(&family,&width)||!readable(job->object,ZF_FILE_BYTES)) {
	ld	hl, #14
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	a, (hl)
	bit	0, c
	jr	NZ, f_dispatch_L00103
	ld	hl, #2
	add	hl, sp
	ex	de, hl
	ld	hl, #0
	add	hl, sp
    reference r657, f_dispatch, 1, f_descriptor, f_descriptor, 0
	call	_descriptor
	or	a, a
	jr	Z, f_dispatch_L00103
    reference r658, f_dispatch, 1, sf_data, _job, 0
	ld	hl, (_job)
	inc	hl
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	de, #0x00b0
    reference r659, f_dispatch, 1, f_readable, f_readable, 0
	call	_readable
	or	a, a
	jr	NZ, f_dispatch_L00104
f_dispatch_L00103:
; fail(0x1ff,0); return;
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x01ff
    reference r660, f_dispatch, 1, f_fail, f_fail, 0
	call	_fail
    reference r661, f_dispatch, 1, f_dispatch, f_dispatch_L00181, 0
	jp	f_dispatch_L00181
f_dispatch_L00104:
; if(!standards()) return;
    reference r662, f_dispatch, 1, f_standards, f_standards, 0
	call	_standards
	or	a, a
    reference r663, f_dispatch, 1, f_dispatch, f_dispatch_L00181, 0
	jp	Z,f_dispatch_L00181
; valid=bound(family,width);
	pop	hl
	pop	de
	push	de
	push	hl
    reference r664, f_dispatch, 1, f_bound, f_bound, 0
	call	_bound
; if(valid&&record->standard!=standard_identity()) {
	ld	-17 (ix), a
	or	a, a
	jr	Z, f_dispatch_L00123
    reference r665, f_dispatch, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0018
	add	hl, de
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	push	bc
    reference r666, f_dispatch, 1, f_standard_identity, f_standard_identity, 0
	call	_standard_identity
	pop	bc
	ld	l, a
	ld	h, #0x00
	cp	a, a
	sbc	hl, bc
	jr	Z, f_dispatch_L00123
; if(job->operation==ZF_ASSIGN&&!standard_identity())valid=0;
    reference r667, f_dispatch, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	a, (hl)
	inc	hl
	ld	c, (hl)
	or	a, c
	jr	NZ, f_dispatch_L00119
    reference r668, f_dispatch, 1, f_standard_identity, f_standard_identity, 0
	call	_standard_identity
	or	a, a
	jr	NZ, f_dispatch_L00119
	ld	-17 (ix), #0x00
	jr	f_dispatch_L00123
f_dispatch_L00119:
; else {fail(0x1ff,0);return;}
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x01ff
    reference r669, f_dispatch, 1, f_fail, f_fail, 0
	call	_fail
    reference r670, f_dispatch, 1, f_dispatch, f_dispatch_L00181, 0
	jp	f_dispatch_L00181
f_dispatch_L00123:
; job=invocation;record=(Record *)USER(job->object);
    reference r671, f_dispatch, 1, sf_data, _job, 0
	ld	hl, (_job)
; if(job->operation!=ZF_STREAM_RETURN) {
	ld	-2 (ix), l
	ld	-1 (ix), h
	ld	a, (hl)
	ld	-16 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-15 (ix), a
; if(valid&&record->standard!=standard_identity()) {
    reference r672, f_dispatch, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	-14 (ix), l
	ld	-13 (ix), h
; if(!valid||record->device<ZF_STANDARD_KBD||record->device>ZF_STANDARD_USR||
	ld	a, -14 (ix)
	add	a, #0x1a
	ld	-12 (ix), a
	ld	a, -13 (ix)
	adc	a, #0x00
	ld	-11 (ix), a
; job->stream!=record->device||job->lookahead>0x1ffu)
	ld	a, -2 (ix)
	add	a, #0x22
	ld	-10 (ix), a
	ld	a, -1 (ix)
	adc	a, #0x00
	ld	-9 (ix), a
; if(job->operation==ZF_STREAM_RETURN) {
	ld	a, -16 (ix)
	sub	a, #0x1d
	or	a, -15 (ix)
    reference r673, f_dispatch, 1, f_dispatch, f_dispatch_L00135, 0
	jp	NZ,f_dispatch_L00135
; if(!valid||record->device<ZF_STANDARD_KBD||record->device>ZF_STANDARD_USR||
	ld	a, -17 (ix)
	or	a, a
	jr	Z, f_dispatch_L00125
	ld	l, -12 (ix)
	ld	h, -11 (ix)
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	sub	a, #0x03
	ld	a, b
	sbc	a, #0x00
	jr	C, f_dispatch_L00125
	ld	a, #0x08
	cp	a, c
	ld	a, #0x00
	sbc	a, b
	jr	C, f_dispatch_L00125
; record->device==ZF_STANDARD_LST||record->device==ZF_STANDARD_AUX||
	ld	a, c
	sub	a, #0x06
	or	a, b
	jr	Z, f_dispatch_L00125
	ld	a, c
	sub	a, #0x07
	or	a, b
	jr	Z, f_dispatch_L00125
; job->stream!=record->device||job->lookahead>0x1ffu)
	ld	l, -10 (ix)
	ld	h, -9 (ix)
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	cp	a, a
	sbc	hl, bc
	jr	NZ, f_dispatch_L00125
	ld	c, -2 (ix)
	ld	b, -1 (ix)
	ld	hl, #36
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	e, c
	ld	d, b
	ld	a, #0xff
	cp	a, e
	ld	a, #0x01
	sbc	a, d
	jr	NC, f_dispatch_L00126
f_dispatch_L00125:
; fail(0x1ff,0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x01ff
    reference r674, f_dispatch, 1, f_fail, f_fail, 0
	call	_fail
    reference r675, f_dispatch, 1, f_dispatch, f_dispatch_L00181, 0
	jp	f_dispatch_L00181
f_dispatch_L00126:
; record->next=(uint8_t)job->lookahead;
	ld	l, -14 (ix)
	ld	h, -13 (ix)
	inc	hl
	ld	(hl), c
; if(valid&&record->standard!=standard_identity()) {
    reference r676, f_dispatch, 2, sf_data, _record, 0
	ld	bc, (_record)
; record->state=(record->state&~LOOKAHEAD)|((job->lookahead&0x100u)?LOOKAHEAD:0);
	ld	hl, #0x0010
	add	hl, bc
	ld	c,l
	ld	b,h
	ld	d, (hl)
	inc	hl
	ld	e, (hl)
	ld	a, d
	and	a, #0xef
	ld	-2 (ix), a
	ld	-1 (ix), e
    reference r677, f_dispatch, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0025
	add	hl, de
	ld	a, (hl)
	rrca
	jr	NC, f_dispatch_L00183
	ld	de, #0x0010
	jr	f_dispatch_L00184
f_dispatch_L00183:
	ld	de, #0x0000
f_dispatch_L00184:
	ld	a, e
	or	a, -2 (ix)
	ld	e, a
	ld	d, -1 (ix)
	ld	a, e
	ld	(bc), a
	inc	bc
	ld	a, d
	ld	(bc), a
; flags();
    reference r678, f_dispatch, 1, f_flags, f_flags, 0
	call	_flags
; return;
    reference r679, f_dispatch, 1, f_dispatch, f_dispatch_L00181, 0
	jp	f_dispatch_L00181
f_dispatch_L00135:
; else if(job->operation==ZF_TEXT_READ) {
	ld	a, -16 (ix)
	ld	-8 (ix), a
	ld	a, -15 (ix)
	ld	-7 (ix), a
; } else if(job->operation==ZF_TEXT_BEGIN||job->operation==ZF_TEXT_READLN||job->operation==ZF_TEXT_WRITELN||
	ld	a, -16 (ix)
	sub	a, #0x1c
	or	a, -15 (ix)
	ld	a, #0x01
	jr	Z, f_dispatch_L00613
	xor	a, a
f_dispatch_L00613:
	ld	-6 (ix), a
	ld	a, -16 (ix)
	sub	a, #0x16
	or	a, -15 (ix)
	ld	a, #0x01
	jr	Z, f_dispatch_L00615
	xor	a, a
f_dispatch_L00615:
	ld	-5 (ix), a
; job->operation==ZF_EOF||(job->operation>=ZF_EOLN&&job->operation<=ZF_SEEKEOLN))
	ld	a, -16 (ix)
	sub	a, #0x09
	or	a, -15 (ix)
	ld	a, #0x01
	jr	Z, f_dispatch_L00617
	xor	a, a
f_dispatch_L00617:
	ld	-4 (ix), a
	ld	a, -16 (ix)
	sub	a, #0x18
	ld	a, -15 (ix)
	sbc	a, #0x00
	ld	a, #0x00
	rla
	ld	-3 (ix), a
	ld	a, #0x1a
	cp	a, -16 (ix)
	ld	a, #0x00
	sbc	a, -15 (ix)
	ld	a, #0x00
	rla
	ld	-2 (ix), a
; else if(job->operation==ZF_TEXT_READ) {
	ld	a, -8 (ix)
	sub	a, #0x14
	or	a, -7 (ix)
	ld	a, #0x01
	jr	Z, f_dispatch_L00619
	xor	a, a
f_dispatch_L00619:
	ld	-1 (ix), a
; if(valid&&record->standard) {
	ld	a, -17 (ix)
	or	a, a
    reference r680, f_dispatch, 1, f_dispatch, f_dispatch_L00164, 0
	jp	Z, f_dispatch_L00164
	ld	c, -14 (ix)
	ld	b, -13 (ix)
	ld	hl, #24
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, b
	or	a, c
    reference r681, f_dispatch, 1, f_dispatch, f_dispatch_L00164, 0
	jp	Z, f_dispatch_L00164
; if(job->operation==ZF_ASSIGN||job->operation==ZF_RESET||job->operation==ZF_REWRITE||job->operation==ZF_CLOSE)
	ld	a, -15 (ix)
	or	a, -16 (ix)
	jr	Z, f_dispatch_L00157
	ld	a, -16 (ix)
	dec	a
	or	a, -15 (ix)
	jr	Z, f_dispatch_L00157
	ld	a, -16 (ix)
	sub	a, #0x02
	or	a, -15 (ix)
	jr	Z, f_dispatch_L00157
	ld	a, -16 (ix)
	sub	a, #0x03
	or	a, -15 (ix)
	jr	NZ, f_dispatch_L00158
f_dispatch_L00157:
; fail(0x22,0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0022
    reference r682, f_dispatch, 1, f_fail, f_fail, 0
	call	_fail
    reference r683, f_dispatch, 1, f_dispatch, f_dispatch_L00159, 0
	jp	f_dispatch_L00159
f_dispatch_L00158:
; else if(record->device) logical_stream();
	ld	l, -12 (ix)
	ld	h, -11 (ix)
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	Z, f_dispatch_L00155
    reference r684, f_dispatch, 1, f_logical_stream, f_logical_stream, 0
	call	_logical_stream
    reference r685, f_dispatch, 1, f_dispatch, f_dispatch_L00159, 0
	jp	f_dispatch_L00159
f_dispatch_L00155:
; else if(job->operation==ZF_TEXT_READ) {
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_dispatch_L00152
; if(text_input_target()) job->stream=record->standard;
    reference r686, f_dispatch, 1, f_text_input_target, f_text_input_target, 0
	call	_text_input_target
	or	a, a
    reference r687, f_dispatch, 1, f_dispatch, f_dispatch_L00159, 0
	jp	Z, f_dispatch_L00159
    reference r688, f_dispatch, 2, sf_data, _job, 0
	ld	bc, (_job)
	ld	hl, #0x0022
	add	hl, bc
	ex	de, hl
    reference r689, f_dispatch, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0018
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	ld	(de), a
	inc	de
	ld	a, b
	ld	(de), a
	jr	f_dispatch_L00159
f_dispatch_L00152:
; } else if(job->operation==ZF_TEXT_WRITE) {
	ld	a, -16 (ix)
	sub	a, #0x15
	or	a, -15 (ix)
	jr	NZ, f_dispatch_L00149
; if(text_output_value()) job->stream=record->standard;
    reference r690, f_dispatch, 1, f_text_output_value, f_text_output_value, 0
	call	_text_output_value
	or	a, a
	jr	Z, f_dispatch_L00159
    reference r691, f_dispatch, 2, sf_data, _job, 0
	ld	bc, (_job)
	ld	hl, #0x0022
	add	hl, bc
	ex	de, hl
    reference r692, f_dispatch, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0018
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	ld	(de), a
	inc	de
	ld	a, b
	ld	(de), a
	jr	f_dispatch_L00159
f_dispatch_L00149:
; } else if(job->operation==ZF_TEXT_BEGIN||job->operation==ZF_TEXT_READLN||job->operation==ZF_TEXT_WRITELN||
	ld	a, -6 (ix)
	or	a, a
	jr	NZ, f_dispatch_L00140
	ld	a, -5 (ix)
	or	a, a
	jr	NZ, f_dispatch_L00140
	ld	a, -16 (ix)
	sub	a, #0x17
	or	a, -15 (ix)
	jr	Z, f_dispatch_L00140
; job->operation==ZF_EOF||(job->operation>=ZF_EOLN&&job->operation<=ZF_SEEKEOLN))
	ld	a, -4 (ix)
	or	a, a
	jr	NZ, f_dispatch_L00140
	bit	0, -3 (ix)
	jr	NZ, f_dispatch_L00141
	bit	0, -2 (ix)
	jr	NZ, f_dispatch_L00141
f_dispatch_L00140:
; job->stream=record->standard;
	ld	l, -10 (ix)
	ld	h, -9 (ix)
	ld	(hl), c
	inc	hl
	ld	(hl), b
	jr	f_dispatch_L00159
f_dispatch_L00141:
; else fail(0x20,0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r693, f_dispatch, 1, f_fail, f_fail, 0
	call	_fail
f_dispatch_L00159:
; flags();return;
    reference r694, f_dispatch, 1, f_flags, f_flags, 0
	call	_flags
    reference r695, f_dispatch, 1, f_dispatch, f_dispatch_L00181, 0
	jp	f_dispatch_L00181
f_dispatch_L00164:
; if(job->operation==ZF_ASSIGN) {
	ld	a, -15 (ix)
	or	a, -16 (ix)
    reference r696, f_dispatch, 1, f_dispatch, f_dispatch_L00172, 0
	jp	NZ, f_dispatch_L00172
; if(!assigned_path()) { fail(0x20,ERR_INVALID_PATH); return; }
    reference r697, f_dispatch, 1, f_assigned_path, f_assigned_path, 0
	call	_assigned_path
	or	a, a
	jr	NZ, f_dispatch_L00167
	ld	a, #0x0b
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r698, f_dispatch, 1, f_fail, f_fail, 0
	call	_fail
    reference r699, f_dispatch, 1, f_dispatch, f_dispatch_L00181, 0
	jp	f_dispatch_L00181
f_dispatch_L00167:
; error=logical_device(transport.request.path);
    reference r700, f_dispatch, 1, sf_data, _transport+1, 0
	ld	hl, #(_transport + 1)
    reference r701, f_dispatch, 1, f_logical_device, f_logical_device, 0
	call	_logical_device
; if(error&&family) { fail(0x20,0); return; }
	ld	-5 (ix), a
	or	a, a
	jr	Z, f_dispatch_L00169
	ld	a, -20 (ix)
	or	a, -21 (ix)
	jr	Z, f_dispatch_L00169
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r702, f_dispatch, 1, f_fail, f_fail, 0
	call	_fail
    reference r703, f_dispatch, 1, f_dispatch, f_dispatch_L00181, 0
	jp	f_dispatch_L00181
f_dispatch_L00169:
; initialize(family,width);
	pop	hl
	pop	de
	push	de
	push	hl
    reference r704, f_dispatch, 1, f_initialize, f_initialize, 0
	call	_initialize
; record->device=error; record->state=error?device_access(error):0; record->next=0;
    reference r705, f_dispatch, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x001a
	add	hl, bc
	ld	a, -5 (ix)
	ld	-2 (ix), a
	ld	-1 (ix), #0x00
	ld	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	(hl), #0x00
    reference r706, f_dispatch, 2, sf_data, _record, 0
	ld	bc, (_record)
	ld	hl, #0x0010
	add	hl, bc
	ld	-4 (ix), l
	ld	-3 (ix), h
	ld	a, -5 (ix)
	or	a, a
	jr	Z, f_dispatch_L00185
	ld	l, -2 (ix)
	ld	h, #0x00
    reference r707, f_dispatch, 1, f_device_access, f_device_access, 0
	call	_device_access
	ld	-1 (ix), a
	jr	f_dispatch_L00186
f_dispatch_L00185:
	ld	-1 (ix), #0x00
f_dispatch_L00186:
	ld	a, -1 (ix)
	ld	-2 (ix), a
	ld	-1 (ix), #0x00
	ld	l, -4 (ix)
	ld	h, -3 (ix)
	ld	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	(hl), #0x00
    reference r708, f_dispatch, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	-4 (ix), l
	ld	-3 (ix), h
	ld	a, -4 (ix)
	add	a, #0x01
	ld	-2 (ix), a
	ld	a, -3 (ix)
	adc	a, #0x00
	ld	-1 (ix), a
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	ld	(hl), #0x00
; strcpy(record->path,transport.request.path);flags();return;
    reference r709, f_dispatch, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	-4 (ix), l
	ld	-3 (ix), h
	ld	a, -4 (ix)
	add	a, #0x30
	ld	-2 (ix), a
	ld	a, -3 (ix)
	adc	a, #0x00
	ld	-1 (ix), a
	ld	e, -2 (ix)
	ld	d, -1 (ix)
    reference r710, f_dispatch, 1, sf_data, _transport+1, 0
	ld	hl, #(_transport + 1)
	xor	a, a
f_dispatch_L00627:
	cp	a, (hl)
	ldi
	jr	NZ, f_dispatch_L00627
    reference r711, f_dispatch, 1, f_flags, f_flags, 0
	call	_flags
    reference r712, f_dispatch, 1, f_dispatch, f_dispatch_L00181, 0
	jp	f_dispatch_L00181
f_dispatch_L00172:
; if(!valid) {
	ld	a, -17 (ix)
	or	a, a
	jr	NZ, f_dispatch_L00174
; fail(family==2?4:job->operation==ZF_READ||job->operation==ZF_TEXT_READ||
	pop	bc
	push	bc
	ld	a, c
	sub	a, #0x02
	or	a, b
	jr	NZ, f_dispatch_L00187
	ld	l, #0x04
	jr	f_dispatch_L00188
f_dispatch_L00187:
	ld	a, -16 (ix)
	sub	a, #0x04
	or	a, -15 (ix)
	jr	Z, f_dispatch_L00192
	bit	0, -1 (ix)
	jr	NZ, f_dispatch_L00192
; job->operation==ZF_TEXT_READLN||job->operation==ZF_TEXT_BEGIN||job->operation==ZF_EOF||
	ld	a, -5 (ix)
	ld	-1 (ix), a
	bit	0, -1 (ix)
	jr	NZ, f_dispatch_L00192
	ld	a, -6 (ix)
	ld	-1 (ix), a
	bit	0, -1 (ix)
	jr	NZ, f_dispatch_L00192
	ld	c, -4 (ix)
	bit	0, c
	jr	NZ, f_dispatch_L00192
; (job->operation>=ZF_EOLN&&job->operation<=ZF_SEEKEOLN)?2:3,0); return;
	bit	0, -3 (ix)
	jr	NZ, f_dispatch_L00189
	bit	0, -2 (ix)
	jr	NZ, f_dispatch_L00189
f_dispatch_L00192:
	ld	-1 (ix), #0x02
	jr	f_dispatch_L00190
f_dispatch_L00189:
	ld	-1 (ix), #0x03
f_dispatch_L00190:
	ld	l, -1 (ix)
f_dispatch_L00188:
	xor	a, a
	ld	h, a
	push	af
	inc	sp
    reference r713, f_dispatch, 1, f_fail, f_fail, 0
	call	_fail
	jr	f_dispatch_L00181
f_dispatch_L00174:
; if(record->device) { logical_stream();flags();return; }
	ld	l, -12 (ix)
	ld	h, -11 (ix)
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	Z, f_dispatch_L00176
    reference r714, f_dispatch, 1, f_logical_stream, f_logical_stream, 0
	call	_logical_stream
    reference r715, f_dispatch, 1, f_flags, f_flags, 0
	call	_flags
	jr	f_dispatch_L00181
f_dispatch_L00176:
; if(job->operation<34&&zf_operations[job->operation])zf_operations[job->operation]();
	ld	a, -8 (ix)
	sub	a, #0x22
	ld	a, -7 (ix)
	sbc	a, #0x00
	jr	NC, f_dispatch_L00178
    reference r716, f_dispatch, 1, sf_data, _zf_operations, 0
	ld	bc, #_zf_operations+0
	ld	l, -16 (ix)
	ld	h, -15 (ix)
	add	hl, hl
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, b
	or	a, c
	jr	Z, f_dispatch_L00178
	ld	l, c
	ld	h, b
    reference r717, f_dispatch, 1, f_sdcc_call_hl, f_sdcc_call_hl, 0
	call	___sdcc_call_hl
	jr	f_dispatch_L00179
f_dispatch_L00178:
; else fail(0x20,ERR_NOT_IMPLEMENTED);
	ld	a, #0x02
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r718, f_dispatch, 1, f_fail, f_fail, 0
	call	_fail
f_dispatch_L00179:
; flags(); /* Reflect validated state changes even if transfer/close failed. */
    reference r719, f_dispatch, 1, f_flags, f_flags, 0
	call	_flags
f_dispatch_L00181:
; }
	ld	sp, ix
	pop	ix
	ret
; void zf_dispatch(FileJob *invocation) ZFILE_CALL {
endfragment f_dispatch

fragment f_zf_dispatch
; Function zf_dispatch
_zf_dispatch:
; native_fd=-1;
    reference r720, f_zf_dispatch, 2, sf_data, _native_fd, 0
	ld	iy, #_native_fd
	ld	0 (iy), #0xff
; dispatch(invocation);
    reference r721, f_zf_dispatch, 1, f_dispatch, f_dispatch, 0
	call	_dispatch
; disk_release();
; }
    reference r722, f_zf_dispatch, 1, f_disk_release, f_disk_release, 0
	jp	_disk_release
; void zf_cleanup(FileCleanup *ending) ZFILE_CALL {
endfragment f_zf_dispatch

fragment f_zf_cleanup
; Function zf_cleanup
_zf_cleanup:
; ending->error=ending->detail=0;
	ld	c, l
	ld	b, h
	inc	bc
	inc	bc
	ld	de, #0x0004
	add	hl, de
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	xor	a, a
	ld	(bc), a
	inc	bc
	ld	(bc), a
; }
	ret
endfragment f_zf_cleanup
