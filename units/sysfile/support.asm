; Support routines for the compiler-private SYSFILE Unit.
; Maintained assembly migrated from the verified backend; the C implementation
; remains the legacy/host behavior oracle, not a production build input.

fragment f_word
; Function word
_word:
; return (uint16_t)USER(at)[0]|((uint16_t)USER(at)[1]<<8);
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
; }
	ret
; static void fail(uint16_t error,uint8_t detail) {
endfragment f_word

fragment f_fail
; Function fail
_fail:
	push	ix
	ld	ix,#0
	add	ix,sp
	ex	de, hl
; if(!job->error) { job->error=error; job->detail=detail; }
    reference r0, f_fail, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	bc, #0x0016
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	dec	hl
	ld	a, b
	or	a, c
	jr	NZ, f_fail_L00103
	ld	(hl), e
	inc	hl
	ld	(hl), d
    reference r1, f_fail, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	bc, #0x0018
	add	hl, bc
	ld	c, 4 (ix)
	xor	a, a
	ld	(hl), c
	inc	hl
	ld	(hl), a
f_fail_L00103:
; }
	pop	ix
	pop	hl
	inc	sp
	jp	(hl)
; static void native_error(uint8_t detail,uint8_t writing) {
endfragment f_fail

fragment f_native_error
; Function native_error
_native_error:
; fail(detail==ERR_NO_SUCH_ENTRY?1:detail==ERR_CANNOT_REGISTER_MORE?0xf3:
	ld	c, a
	sub	a, #0x04
	jr	NZ, f_native_error_L00103
	ld	l, #0x01
	jr	f_native_error_L00104
f_native_error_L00103:
	ld	a, c
	sub	a, #0x14
	jr	NZ, f_native_error_L00105
	ld	l, #0xf3
	jr	f_native_error_L00106
f_native_error_L00105:
; detail==ERR_READ_ONLY||detail==ERR_BAD_MODE?(writing==2?0x20:writing?3:2):
	ld	a,c
	cp	a,#0x12
	jr	Z, f_native_error_L00110
	sub	a, #0x13
	jr	NZ, f_native_error_L00107
f_native_error_L00110:
	ld	a, l
	sub	a, #0x02
	jr	NZ, f_native_error_L00112
	ld	l, #0x20
	jr	f_native_error_L00108
f_native_error_L00112:
	ld	a, l
	or	a, a
	ld	l, #0x03
	jr	NZ, f_native_error_L00108
	ld	l, #0x02
	jr	f_native_error_L00108
f_native_error_L00107:
; detail==ERR_NOT_A_FILE||detail==ERR_NOT_SUPPORTED||detail==ERR_NOT_IMPLEMENTED||
	ld	a,c
	cp	a,#0x18
	jr	Z, f_native_error_L00119
; detail==ERR_ALREADY_EXIST?0x20:writing?0xf0:0x99,detail);
	cp	a,#0x03
	jr	Z, f_native_error_L00119
	cp	a,#0x02
	jr	Z, f_native_error_L00119
	sub	a, #0x0f
	jr	NZ, f_native_error_L00116
f_native_error_L00119:
	ld	l, #0x20
	jr	f_native_error_L00117
f_native_error_L00116:
	ld	a, l
	or	a, a
	ld	l, #0xf0
	jr	NZ, f_native_error_L00128
	ld	l, #0x99
f_native_error_L00128:
f_native_error_L00117:
f_native_error_L00108:
f_native_error_L00106:
f_native_error_L00104:
	ld	h, #0x00
	ld	a, c
	push	af
	inc	sp
    reference r2, f_native_error, 1, f_fail, f_fail, 0
	call	_fail
; }
	ret
; static int8_t open_path(const char *path,uint8_t flags) {
endfragment f_native_error

fragment f_open_path
; Function open_path
_open_path:
	push	ix
	ld	ix,#0
	add	ix,sp
	ex	de, hl
; transport.request.flags=flags;
    reference r3, f_open_path, 1, sf_data, _transport, 0
	ld	bc, #_transport+0
	ld	a, 4 (ix)
	ld	(bc), a
; if(path!=transport.request.path)strcpy(transport.request.path,path);
	inc	bc
	ld	l, c
	ld	h, b
	cp	a, a
	sbc	hl, de
	jr	Z, f_open_path_L00103
	ld	l, c
	ld	h, b
	push	bc
	ex	de, hl
	xor	a, a
f_open_path_L00118:
	cp	a, (hl)
	ldi
	jr	NZ, f_open_path_L00118
	pop	bc
; if(!MEMORY_MODE) fd=open(transport.request.path,flags);
f_open_path_L00103:
	ld	a, 4 (ix)
	push	af
	inc	sp
	ld	l, c
	ld	h, b
    reference r4, f_open_path, 1, f_open, f_open, 0
	call	_open
; return fd;
; }
	pop	ix
	pop	hl
	inc	sp
	jp	(hl)
; static int8_t open_file(uint8_t flags) { return open_path(record->path,flags); }
endfragment f_open_path

fragment f_open_file
; Function open_file
_open_file:
	ld	b, a
    reference r5, f_open_file, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0030
	add	hl, de
	push	bc
	inc	sp
    reference r6, f_open_file, 1, f_open_path, f_open_path, 0
	call	_open_path
	ret
; static uint8_t stat_file(void) {
endfragment f_open_file

fragment f_stat_file
; Function stat_file
_stat_file:
; strcpy(transport.request.path,record->path);return stat(transport.request.path,&info);
    reference r7, f_stat_file, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0030
	add	hl, bc
    reference r8, f_stat_file, 1, sf_data, _transport+1, 0
	ld	de, #(_transport + 1)
	xor	a, a
f_stat_file_L00103:
	cp	a, (hl)
	ldi
	jr	NZ, f_stat_file_L00103
    reference r9, f_stat_file, 1, sf_data, _info, 0
	ld	de, #_info
    reference r10, f_stat_file, 1, sf_data, _transport+1, 0
	ld	hl, #(_transport + 1)
; }
    reference r11, f_stat_file, 1, f_stat, f_stat, 0
	jp	_stat
; static uint8_t remove_file(void) {
endfragment f_stat_file

fragment f_remove_file
; Function remove_file
_remove_file:
; strcpy(transport.request.path,record->path);return rm(transport.request.path);
    reference r12, f_remove_file, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0030
	add	hl, bc
    reference r13, f_remove_file, 1, sf_data, _transport+1, 0
	ld	de, #(_transport + 1)
	xor	a, a
f_remove_file_L00103:
	cp	a, (hl)
	ldi
	jr	NZ, f_remove_file_L00103
    reference r14, f_remove_file, 1, sf_data, _transport+1, 0
	ld	hl, #(_transport + 1)
; }
    reference r15, f_remove_file, 1, f_rm, f_rm, 0
	jp	_rm
; static uint8_t close_file(uint8_t fd) {
endfragment f_remove_file

fragment f_close_file
; Function close_file
_close_file:
; return MEMORY_MODE?zf_service(3,fd):close((int8_t)fd);
	ld	c, a
; }
    reference r16, f_close_file, 1, f_close, f_close, 0
	jp	_close
; static uint8_t within(uint16_t at,uint16_t size,uint16_t low,uint16_t high) {
endfragment f_close_file
