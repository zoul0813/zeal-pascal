; Transport routines for the compiler-private SYSFILE Unit.
; Maintained assembly migrated from the verified backend; the C implementation
; remains the legacy/host behavior oracle, not a production build input.

fragment f_bookkeeping_status
; Function bookkeeping_status
_bookkeeping_status:
	push	ix
	ld	ix,#0
	add	ix,sp
	ld	iy, #-8
	add	iy, sp
	ld	sp, iy
	ld	c, l
	ld	b, h
; if(size<=position) return 0;
	ld	a, 4 (ix)
	sub	a, e
	ld	a, 5 (ix)
	sbc	a, d
	ld	a, 6 (ix)
	sbc	a, c
	ld	a, 7 (ix)
	sbc	a, b
	jr	C, f_bookkeeping_status_L00102
	xor	a, a
    reference r94, f_bookkeeping_status, 1, f_bookkeeping_status, f_bookkeeping_status_L00109, 0
	jp	f_bookkeeping_status_L00109
f_bookkeeping_status_L00102:
; if((size>>24)!=(position>>24)) return (uint8_t)(size>>24);
	ld	-8 (ix), b
	xor	a, a
	ld	-7 (ix), a
	ld	-6 (ix), a
	ld	-5 (ix), a
	ld	a, 7 (ix)
	ld	-4 (ix), a
	xor	a, a
	ld	-3 (ix), a
	ld	-2 (ix), a
	ld	-1 (ix), a
	ld	a, -8 (ix)
	sub	a, -4 (ix)
	jr	NZ, f_bookkeeping_status_L00143
	xor	a, a
	sub	a, -3 (ix)
	jr	NZ, f_bookkeeping_status_L00143
	xor	a, a
	sub	a, -2 (ix)
	jr	NZ, f_bookkeeping_status_L00143
	xor	a, a
	sub	a, -1 (ix)
	jr	Z, f_bookkeeping_status_L00104
f_bookkeeping_status_L00143:
	ld	a, b
    reference r95, f_bookkeeping_status, 1, f_bookkeeping_status, f_bookkeeping_status_L00109, 0
	jp	f_bookkeeping_status_L00109
f_bookkeeping_status_L00104:
; if((size>>16)!=(position>>16)) return (uint8_t)(size>>16);
	inc	sp
	inc	sp
	push	bc
	xor	a, a
	ld	-6 (ix), a
	ld	-5 (ix), a
	ld	a, 6 (ix)
	ld	-4 (ix), a
	ld	a, 7 (ix)
	ld	-3 (ix), a
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
	ld	a, -4 (ix)
	sub	a, -8 (ix)
	jr	NZ, f_bookkeeping_status_L00148
	ld	a, -3 (ix)
	sub	a, -7 (ix)
	jr	NZ, f_bookkeeping_status_L00148
	xor	a, a
	sub	a, -6 (ix)
	jr	NZ, f_bookkeeping_status_L00148
	xor	a, a
	sub	a, -5 (ix)
	jr	Z, f_bookkeeping_status_L00106
f_bookkeeping_status_L00148:
	ld	a, c
	jr	f_bookkeeping_status_L00109
f_bookkeeping_status_L00106:
; if((size>>8)!=(position>>8)) return (uint8_t)(size>>8);
	ld	-8 (ix), d
	ld	-7 (ix), c
	ld	-6 (ix), b
	ld	-5 (ix), #0x00
	ld	a, 5 (ix)
	ld	-4 (ix), a
	ld	a, 6 (ix)
	ld	-3 (ix), a
	ld	a, 7 (ix)
	ld	-2 (ix), a
	ld	-1 (ix), #0x00
	ld	a, -8 (ix)
	sub	a, -4 (ix)
	jr	NZ, f_bookkeeping_status_L00153
	ld	a, -7 (ix)
	sub	a, -3 (ix)
	jr	NZ, f_bookkeeping_status_L00153
	ld	a, -6 (ix)
	sub	a, -2 (ix)
	jr	NZ, f_bookkeeping_status_L00153
	xor	a, a
	sub	a, -1 (ix)
	jr	Z, f_bookkeeping_status_L00108
f_bookkeeping_status_L00153:
	ld	a, d
	jr	f_bookkeeping_status_L00109
f_bookkeeping_status_L00108:
; return 0;
	xor	a, a
f_bookkeeping_status_L00109:
; }
	ld	sp, ix
	pop	ix
	pop	hl
	pop	bc
	pop	bc
	jp	(hl)
; static uint8_t write_bytes(const uint8_t *data,uint16_t *size) {
endfragment f_bookkeeping_status

fragment f_write_bytes
; Function write_bytes
_write_bytes:
	push	ix
	ld	ix,#0
	add	ix,sp
	ld	iy, #-25
	add	iy, sp
	ld	sp, iy
	ld	-2 (ix), l
	ld	-1 (ix), h
	ld	c, e
	ld	b, d
; uint16_t wanted=*size;
	ld	a, (bc)
	ld	-13 (ix), a
	inc	bc
	ld	a, (bc)
	ld	-12 (ix), a
	dec	bc
; uint8_t error=write((int8_t)native_fd,data,size);
	ld	e, -2 (ix)
	ld	d, -1 (ix)
	push	bc
	push	bc
    reference r96, f_write_bytes, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r97, f_write_bytes, 1, f_write, f_write, 0
	call	_write
	ld	-11 (ix), a
	pop	bc
; int32_t next=0,end=0,restored;
	xor	a, a
	ld	-25 (ix), a
	ld	-24 (ix), a
	ld	-23 (ix), a
	ld	-22 (ix), a
	xor	a, a
	ld	-21 (ix), a
	ld	-20 (ix), a
	ld	-19 (ix), a
	ld	-18 (ix), a
; if(record->device) return error;
    reference r98, f_write_bytes, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x001a
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, d
	or	a, e
	jr	Z, f_write_bytes_L00102
	ld	a, -11 (ix)
    reference r99, f_write_bytes, 1, f_write_bytes, f_write_bytes_L00121, 0
	jp	f_write_bytes_L00121
f_write_bytes_L00102:
; if(!error) {
	ld	a, -11 (ix)
	or	a, a
	jr	NZ, f_write_bytes_L00106
; if(*size<=wanted) native_position+=*size;
	ld	l, c
	ld	h, b
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, -13 (ix)
	sub	a, c
	ld	a, -12 (ix)
	sbc	a, b
	jr	C, f_write_bytes_L00104
	ld	de, #0x0000
	ld	a, c
    reference r100, f_write_bytes, 1, sf_data, _native_position, 0
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
f_write_bytes_L00104:
; return 0;
	xor	a, a
    reference r101, f_write_bytes, 1, f_write_bytes, f_write_bytes_L00121, 0
	jp	f_write_bytes_L00121
f_write_bytes_L00106:
; if(!wanted||seek((int8_t)native_fd,&next,SEEK_CUR)) return error;
	ld	a, -12 (ix)
	or	a, -13 (ix)
	jr	Z, f_write_bytes_L00107
	push	bc
	ld	a, #0x01
	push	af
	inc	sp
	ld	hl, #3
	add	hl, sp
	ex	de, hl
    reference r102, f_write_bytes, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r103, f_write_bytes, 1, f_seek, f_seek, 0
	call	_seek
	pop	bc
	or	a, a
	jr	Z, f_write_bytes_L00108
f_write_bytes_L00107:
	ld	a, -11 (ix)
    reference r104, f_write_bytes, 1, f_write_bytes, f_write_bytes_L00121, 0
	jp	f_write_bytes_L00121
f_write_bytes_L00108:
; if((uint32_t)next<=native_position) return error;
	push	bc
	ld	hl, #17
	add	hl, sp
	ex	de, hl
	ld	hl, #2
	add	hl, sp
	ld	bc, #4
	ldir
	pop	bc
    reference r105, f_write_bytes, 1, sf_data, _native_position, 0
	ld	hl, #_native_position
	ld	a, (hl)
	sub	a, -10 (ix)
	inc	hl
	ld	a, (hl)
	sbc	a, -9 (ix)
	inc	hl
	ld	a, (hl)
	sbc	a, -8 (ix)
	inc	hl
	ld	a, (hl)
	sbc	a, -7 (ix)
	jr	C, f_write_bytes_L00111
	ld	a, -11 (ix)
    reference r106, f_write_bytes, 1, f_write_bytes, f_write_bytes_L00121, 0
	jp	f_write_bytes_L00121
f_write_bytes_L00111:
; advanced=(uint32_t)next-native_position;
    reference r107, f_write_bytes, 1, sf_data, _native_position, 0
	ld	hl, #_native_position
	ld	a, -10 (ix)
	sub	a, (hl)
	ld	-6 (ix), a
	ld	a, -9 (ix)
	inc	hl
	sbc	a, (hl)
	ld	-5 (ix), a
	ld	a, -8 (ix)
	inc	hl
	sbc	a, (hl)
	ld	-4 (ix), a
	ld	a, -7 (ix)
	inc	hl
	sbc	a, (hl)
	ld	-3 (ix), a
; if(advanced>wanted) return error;
	ld	e, -13 (ix)
	ld	d, -12 (ix)
	ld	hl, #0x0000
	ld	a, e
	sub	a, -6 (ix)
	ld	a, d
	sbc	a, -5 (ix)
	ld	a, l
	sbc	a, -4 (ix)
	ld	a, h
	sbc	a, -3 (ix)
	jr	NC, f_write_bytes_L00113
	ld	a, -11 (ix)
    reference r108, f_write_bytes, 1, f_write_bytes, f_write_bytes_L00121, 0
	jp	f_write_bytes_L00121
f_write_bytes_L00113:
; if(seek((int8_t)native_fd,&end,SEEK_END)) return error;
	push	bc
	ld	a, #0x02
	push	af
	inc	sp
	ld	hl, #7
	add	hl, sp
	ex	de, hl
    reference r109, f_write_bytes, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r110, f_write_bytes, 1, f_seek, f_seek, 0
	call	_seek
	pop	bc
	or	a, a
	jr	Z, f_write_bytes_L00115
	ld	a, -11 (ix)
    reference r111, f_write_bytes, 1, f_write_bytes, f_write_bytes_L00121, 0
	jp	f_write_bytes_L00121
f_write_bytes_L00115:
; restored=next;
	push	bc
	ld	hl, #10
	add	hl, sp
	ex	de, hl
	ld	hl, #2
	add	hl, sp
	ld	bc, #4
	ldir
	xor	a, a
	push	af
	inc	sp
	ld	hl, #11
	add	hl, sp
	ex	de, hl
    reference r112, f_write_bytes, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r113, f_write_bytes, 1, f_seek, f_seek, 0
	call	_seek
	pop	bc
	or	a, a
	jr	NZ, f_write_bytes_L00116
	ld	a, -17 (ix)
	sub	a, -25 (ix)
	jr	NZ, f_write_bytes_L00186
	ld	a, -16 (ix)
	sub	a, -24 (ix)
	jr	NZ, f_write_bytes_L00186
	ld	a, -15 (ix)
	sub	a, -23 (ix)
	jr	NZ, f_write_bytes_L00186
	ld	a, -14 (ix)
	sub	a, -22 (ix)
	jr	Z, f_write_bytes_L00117
f_write_bytes_L00186:
f_write_bytes_L00116:
	ld	a, -11 (ix)
	jr	f_write_bytes_L00121
f_write_bytes_L00117:
; if(bookkeeping_status((uint32_t)end,(uint32_t)next)!=error) return error;
	push	bc
	ld	hl, #17
	add	hl, sp
	ex	de, hl
	ld	hl, #2
	add	hl, sp
	ld	bc, #4
	ldir
	pop	bc
	ld	e, -21 (ix)
	ld	d, -20 (ix)
	ld	l, -19 (ix)
	ld	h, -18 (ix)
	push	bc
	push	hl
	ld	l, -8 (ix)
	ld	h, -7 (ix)
	ex	(sp), hl
	push	hl
	ld	l, -10 (ix)
	ld	h, -9 (ix)
	ex	(sp), hl
    reference r114, f_write_bytes, 1, f_bookkeeping_status, f_bookkeeping_status, 0
	call	_bookkeeping_status
	ld	e, a
	pop	bc
	ld	a, -11 (ix)
	sub	a, e
	jr	Z, f_write_bytes_L00120
	ld	a, -11 (ix)
	jr	f_write_bytes_L00121
f_write_bytes_L00120:
; *size=(uint16_t)advanced; native_position=(uint32_t)next;
	ld	a, -6 (ix)
	ld	d, -5 (ix)
	ld	(bc), a
	inc	bc
	ld	a, d
	ld	(bc), a
    reference r115, f_write_bytes, 1, sf_data, _native_position, 0
	ld	de, #_native_position
	ld	hl, #0
	add	hl, sp
	ld	bc, #4
	ldir
; return 0;
	xor	a, a
f_write_bytes_L00121:
; }
	ld	sp, ix
	pop	ix
	ret
; static uint16_t transfer(uint16_t address,uint16_t count,uint8_t writing) {
endfragment f_write_bytes

fragment f_transfer
; Function transfer
_transfer:
	push	ix
	ld	ix,#0
	add	ix,sp
	ld	iy, #-14
	add	iy, sp
	ld	sp, iy
	ld	-4 (ix), l
	ld	-3 (ix), h
	ld	-6 (ix), e
	ld	-5 (ix), d
; uint16_t done=0,size,n;
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
; while(done<count) {
f_transfer_L00118:
	ld	a, -2 (ix)
	sub	a, -6 (ix)
	ld	a, -1 (ix)
	sbc	a, -5 (ix)
    reference r116, f_transfer, 1, f_transfer, f_transfer_L00120, 0
	jp	NC, f_transfer_L00120
; n=count-done; if(n>sizeof(bytes)) n=sizeof(bytes);
	ld	a, -6 (ix)
	sub	a, -2 (ix)
	ld	-12 (ix), a
	ld	a, -5 (ix)
	sbc	a, -1 (ix)
	ld	-11 (ix), a
	ld	a, #0x80
	cp	a, -12 (ix)
	ld	a, #0x00
	sbc	a, -11 (ix)
	jr	NC, f_transfer_L00102
	ld	-12 (ix), #0x80
	ld	-11 (ix), #0
f_transfer_L00102:
; if(writing)memcpy(bytes,USER(address+done),n);
	ld	a, -4 (ix)
	add	a, -2 (ix)
	ld	-8 (ix), a
	ld	a, -3 (ix)
	adc	a, -1 (ix)
	ld	-7 (ix), a
	ld	a, -8 (ix)
	ld	-10 (ix), a
	ld	a, -7 (ix)
	ld	-9 (ix), a
	ld	a, 4 (ix)
	or	a, a
	jr	Z, f_transfer_L00104
	ld	a, -12 (ix)
	ld	-8 (ix), a
	ld	a, -11 (ix)
	ld	-7 (ix), a
    reference r117, f_transfer, 1, sf_data, _bytes, 0
	ld	de, #_bytes
	ld	l, -10 (ix)
	ld	h, -9 (ix)
	ld	c, -8 (ix)
	ld	b, -7 (ix)
	ld	a, b
	or	a, c
	jr	Z, f_transfer_L00218
	ldir
f_transfer_L00218:
f_transfer_L00104:
; size=n;
	ld	a, -12 (ix)
	ld	-14 (ix), a
	ld	a, -11 (ix)
	ld	-13 (ix), a
; error=writing?write_bytes(bytes,&size):read((int8_t)native_fd,bytes,&size);
	ld	a, 4 (ix)
	or	a, a
	jr	Z, f_transfer_L00123
	ld	hl, #0
	add	hl, sp
	ex	de, hl
    reference r118, f_transfer, 1, sf_data, _bytes, 0
	ld	hl, #_bytes
    reference r119, f_transfer, 1, f_write_bytes, f_write_bytes, 0
	call	_write_bytes
	jr	f_transfer_L00124
f_transfer_L00123:
	ld	hl, #0
	add	hl, sp
	push	hl
    reference r120, f_transfer, 1, sf_data, _bytes, 0
	ld	de, #_bytes
    reference r121, f_transfer, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r122, f_transfer, 1, f_read, f_read, 0
	call	_read
f_transfer_L00124:
	ld	c, a
; if(error) { native_error(error,writing); break; }
	or	a, a
	jr	Z, f_transfer_L00106
	ld	l, 4 (ix)
	ld	a, c
    reference r123, f_transfer, 1, f_native_error, f_native_error, 0
	call	_native_error
    reference r124, f_transfer, 1, f_transfer, f_transfer_L00120, 0
	jp	f_transfer_L00120
f_transfer_L00106:
; if(size>n) { fail(writing?0xf0:0x99,ERR_INVALID_PARAMETER); break; }
	ld	a, -12 (ix)
	sub	a, -14 (ix)
	ld	a, -11 (ix)
	sbc	a, -13 (ix)
	jr	NC, f_transfer_L00108
	ld	a, 4 (ix)
	or	a, a
	jr	Z, f_transfer_L00125
	ld	-7 (ix), #0xf0
	jr	f_transfer_L00126
f_transfer_L00125:
	ld	-7 (ix), #0x99
f_transfer_L00126:
	ld	l, -7 (ix)
	ld	h, #0x00
	ld	a, #0x06
	push	af
	inc	sp
    reference r125, f_transfer, 1, f_fail, f_fail, 0
	call	_fail
    reference r126, f_transfer, 1, f_transfer, f_transfer_L00120, 0
	jp	f_transfer_L00120
f_transfer_L00108:
; if(!writing&&size)memcpy(USER(address+done),bytes,size);
	ld	a, 4 (ix)
	or	a, a
	jr	NZ, f_transfer_L00110
	ld	a, -13 (ix)
	or	a, -14 (ix)
	jr	Z, f_transfer_L00110
	ld	a, -14 (ix)
	ld	-8 (ix), a
	ld	a, -13 (ix)
	ld	-7 (ix), a
	ld	e, -10 (ix)
	ld	d, -9 (ix)
    reference r127, f_transfer, 1, sf_data, _bytes, 0
	ld	hl, #_bytes
	ld	c, -8 (ix)
	ld	b, -7 (ix)
	ld	a, b
	or	a, c
	jr	Z, f_transfer_L00219
	ldir
f_transfer_L00219:
f_transfer_L00110:
; if(!writing) native_position+=size;
	ld	a, 4 (ix)
	or	a, a
	jr	NZ, f_transfer_L00113
	ld	a, -14 (ix)
	ld	-10 (ix), a
	ld	a, -13 (ix)
	ld	-9 (ix), a
	xor	a, a
	ld	-8 (ix), a
	ld	-7 (ix), a
    reference r128, f_transfer, 1, sf_data, _native_position, 0
	ld	hl, #_native_position
	ld	a, (hl)
	add	a, -10 (ix)
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, -9 (ix)
	ld	(hl), a
	inc	hl
	ld	a, (hl)
	adc	a, -8 (ix)
	ld	(hl), a
	jr	NC, f_transfer_L00220
	inc	hl
	inc	(hl)
f_transfer_L00220:
f_transfer_L00113:
; done+=size;
	ld	a, -2 (ix)
	add	a, -14 (ix)
	ld	-2 (ix), a
	ld	a, -1 (ix)
	adc	a, -13 (ix)
	ld	-1 (ix), a
; if(!size) { if(writing) fail(0xf0,0); break; }
	ld	a, -13 (ix)
	or	a, -14 (ix)
    reference r129, f_transfer, 1, f_transfer, f_transfer_L00118, 0
	jp	NZ, f_transfer_L00118
	ld	a, 4 (ix)
	or	a, a
	jr	Z, f_transfer_L00120
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x00f0
    reference r130, f_transfer, 1, f_fail, f_fail, 0
	call	_fail
f_transfer_L00120:
; return done;
	ld	e, -2 (ix)
	ld	d, -1 (ix)
; }
	ld	sp, ix
	pop	ix
	pop	hl
	inc	sp
	jp	(hl)
; static uint8_t seek_file(uint32_t at) {
endfragment f_transfer

fragment f_seek_file
; Function seek_file
_seek_file:
	push	ix
	ld	ix,#0
	add	ix,sp
	ld	iy, #-8
	add	iy, sp
	ld	sp, iy
	ld	-4 (ix), e
	ld	-3 (ix), d
	ld	-2 (ix), l
	ld	-1 (ix), h
; uint32_t requested=at;
	ld	hl, #0
	add	hl, sp
	ex	de, hl
	ld	hl, #4
	add	hl, sp
	ld	bc, #4
	ldir
; uint8_t error=seek((int8_t)native_fd,(int32_t *)&at,SEEK_SET);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #5
	add	hl, sp
	ex	de, hl
    reference r131, f_seek_file, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r132, f_seek_file, 1, f_seek, f_seek, 0
	call	_seek
	ld	c, a
; if(error) native_error(error,0);
	or	a, a
	jr	Z, f_seek_file_L00105
	ld	l, #0x00
	ld	a, c
    reference r133, f_seek_file, 1, f_native_error, f_native_error, 0
	call	_native_error
	jr	f_seek_file_L00106
f_seek_file_L00105:
; else if(at!=requested) fail(0x99,0); /* native seek clamps at EOF */
	ld	a, -4 (ix)
	sub	a, -8 (ix)
	jr	NZ, f_seek_file_L00123
	ld	a, -3 (ix)
	sub	a, -7 (ix)
	jr	NZ, f_seek_file_L00123
	ld	a, -2 (ix)
	sub	a, -6 (ix)
	jr	NZ, f_seek_file_L00123
	ld	a, -1 (ix)
	sub	a, -5 (ix)
	jr	Z, f_seek_file_L00102
f_seek_file_L00123:
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0099
    reference r134, f_seek_file, 1, f_fail, f_fail, 0
	call	_fail
	jr	f_seek_file_L00106
f_seek_file_L00102:
; else native_position=at;
    reference r135, f_seek_file, 1, sf_data, _native_position, 0
	ld	de, #_native_position
	ld	hl, #4
	add	hl, sp
	ld	bc, #4
	ldir
f_seek_file_L00106:
; return !job->error;
    reference r136, f_seek_file, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0016
	add	hl, de
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	sub	a, #0x01
	ld	a, #0x00
	rla
; }
	ld	sp, ix
	pop	ix
	ret
; static uint8_t header(uint8_t writing) {
endfragment f_seek_file

fragment f_header
; Function header
_header:
    reference r137, f_header, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	ld	hl, #-11
	add	hl, sp
	ld	sp, hl
	ld	-1 (ix), a
; uint16_t size,done=0;
	xor	a, a
	ld	-3 (ix), a
	ld	-2 (ix), a
; if(!seek_file(0)) return 0;
	ld	de, #0x0000
	ld	h, d
	ld	l, e
    reference r138, f_header, 1, f_seek_file, f_seek_file, 0
	call	_seek_file
	ld	-4 (ix), a
	or	a,a
    reference r139, f_header, 1, f_header, f_header_L00120, 0
	jp	Z,f_header_L00120
; if(writing) {
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_header_L00134
; bytes[0]=(uint8_t)record->count; bytes[1]=(uint8_t)(record->count>>8);
    reference r140, f_header, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	-5 (ix), l
	ld	-4 (ix), h
	ld	de, #0x0004
	add	hl, de
	ld	a, (hl)
    reference r141, f_header, 1, sf_data, _bytes, 0
	ld	(#_bytes),a
    reference r142, f_header, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	-5 (ix), l
	ld	-4 (ix), h
	ld	de, #0x0004
	add	hl, de
	ld	a, (hl)
	ld	-5 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-4 (ix), a
    reference r143, f_header, 1, sf_data, _bytes+1, 0
	ld	(#(_bytes + 1)),a
; bytes[2]=(uint8_t)record->width; bytes[3]=(uint8_t)(record->width>>8);
    reference r144, f_header, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	-5 (ix), l
	ld	-4 (ix), h
	ld	de, #0x0006
	add	hl, de
	ld	a, (hl)
    reference r145, f_header, 1, sf_data, _bytes+2, 0
	ld	(#(_bytes + 2)),a
    reference r146, f_header, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	-5 (ix), l
	ld	-4 (ix), h
	ld	de, #0x0006
	add	hl, de
	ld	a, (hl)
	ld	-5 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-4 (ix), a
    reference r147, f_header, 1, sf_data, _bytes+3, 0
	ld	(#(_bytes + 3)),a
; while(done<4) {
f_header_L00134:
f_header_L00112:
	ld	a, -3 (ix)
	ld	b, -2 (ix)
	sub	a, #0x04
	ld	a, b
	sbc	a, #0x00
    reference r148, f_header, 1, f_header, f_header_L00114, 0
	jp	NC, f_header_L00114
; size=4-done;
	ld	a, #0x04
	sub	a, -3 (ix)
	ld	-7 (ix), a
	sbc	a, a
	sub	a, -2 (ix)
	ld	-6 (ix), a
	ld	a, -7 (ix)
	ld	-11 (ix), a
	ld	a, -6 (ix)
	ld	-10 (ix), a
; error=writing?write_bytes(bytes+done,&size):read((int8_t)native_fd,bytes+done,&size);
    reference r149, f_header, 1, sf_data, _bytes, 1
	ld	a, #<(_bytes)
	add	a, -3 (ix)
	ld	-5 (ix), a
    reference r150, f_header, 1, sf_data, _bytes, 2
	ld	a, #>(_bytes)
	adc	a, -2 (ix)
	ld	-4 (ix), a
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_header_L00122
	ld	hl, #0
	add	hl, sp
	ex	de, hl
	ld	l, -5 (ix)
	ld	h, -4 (ix)
    reference r151, f_header, 1, f_write_bytes, f_write_bytes, 0
	call	_write_bytes
	ld	-4 (ix), a
	jr	f_header_L00123
f_header_L00122:
	ld	hl, #0
	add	hl, sp
	push	hl
	ld	e, -5 (ix)
	ld	d, -4 (ix)
    reference r152, f_header, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r153, f_header, 1, f_read, f_read, 0
	call	_read
	ld	-4 (ix), a
f_header_L00123:
	ld	c, -4 (ix)
; if(error) { native_error(error,writing); return 0; }
	ld	a, -4 (ix)
	or	a, a
	jr	Z, f_header_L00106
	ld	l, -1 (ix)
	ld	a, c
    reference r154, f_header, 1, f_native_error, f_native_error, 0
	call	_native_error
	xor	a, a
    reference r155, f_header, 1, f_header, f_header_L00120, 0
	jp	f_header_L00120
f_header_L00106:
; if(!size||size>4-done) { fail(writing?0xf0:0x99,0); return 0; }
	ld	a, -10 (ix)
	or	a, -11 (ix)
	jr	Z, f_header_L00107
	ld	a, -7 (ix)
	sub	a, -11 (ix)
	ld	a, -6 (ix)
	sbc	a, -10 (ix)
	jr	NC, f_header_L00108
f_header_L00107:
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_header_L00124
	ld	-2 (ix), #0xf0
	jr	f_header_L00125
f_header_L00124:
	ld	-2 (ix), #0x99
f_header_L00125:
	ld	l, -2 (ix)
	xor	a, a
	ld	h, a
	push	af
	inc	sp
    reference r156, f_header, 1, f_fail, f_fail, 0
	call	_fail
	xor	a, a
    reference r157, f_header, 1, f_header, f_header_L00120, 0
	jp	f_header_L00120
f_header_L00108:
; if(!writing) native_position+=size;
	ld	a, -1 (ix)
	or	a, a
	jr	NZ, f_header_L00111
	ld	a, -11 (ix)
	ld	-7 (ix), a
	ld	a, -10 (ix)
	ld	-6 (ix), a
	xor	a, a
	ld	-5 (ix), a
	ld	-4 (ix), a
	ld	a, -7 (ix)
    reference r158, f_header, 1, sf_data, _native_position, 0
	ld	hl, #_native_position
	add	a, (hl)
	ld	(hl), a
	ld	a, -6 (ix)
	inc	hl
	adc	a, (hl)
	ld	(hl), a
	inc	hl
	ld	a, #0x00
	adc	a, (hl)
	ld	(hl), a
	inc	hl
	ld	a, #0x00
	adc	a, (hl)
	ld	(hl), a
f_header_L00111:
; done+=size;
	ld	a, -3 (ix)
	add	a, -11 (ix)
	ld	-3 (ix), a
	ld	a, -2 (ix)
	adc	a, -10 (ix)
	ld	-2 (ix), a
    reference r159, f_header, 1, f_header, f_header_L00112, 0
	jp	f_header_L00112
f_header_L00114:
; if(!writing) {
	ld	a, -1 (ix)
	or	a, a
    reference r160, f_header, 1, f_header, f_header_L00119, 0
	jp	NZ, f_header_L00119
; if(((uint16_t)bytes[2]|((uint16_t)bytes[3]<<8))!=record->width) fail(0x90,0);
    reference r161, f_header, 1, sf_data, _bytes+2, 0
	ld	a, (#_bytes + 2)
	ld	-2 (ix), a
	ld	-5 (ix), a
	ld	-4 (ix), #0x00
    reference r162, f_header, 1, sf_data, _bytes+3, 0
	ld	a, (#_bytes + 3)
	ld	-2 (ix), a
	ld	-3 (ix), a
	ld	-2 (ix), #0x00
	ld	a, -3 (ix)
	ld	-2 (ix), a
	ld	-3 (ix), #0x00
	ld	a, -5 (ix)
	ld	-7 (ix), a
	ld	a, -2 (ix)
	ld	-6 (ix), a
; bytes[0]=(uint8_t)record->count; bytes[1]=(uint8_t)(record->count>>8);
    reference r163, f_header, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	-3 (ix), l
	ld	-2 (ix), h
; if(((uint16_t)bytes[2]|((uint16_t)bytes[3]<<8))!=record->width) fail(0x90,0);
	ld	a, -3 (ix)
	ld	-5 (ix), a
	ld	a, -2 (ix)
	ld	-4 (ix), a
	ld	l, -5 (ix)
	ld	h, -4 (ix)
	ld	de, #0x0006
	add	hl, de
	ld	a, (hl)
	ld	-5 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-4 (ix), a
	ld	a, -7 (ix)
	sub	a, -5 (ix)
	jr	NZ, f_header_L00203
	ld	a, -6 (ix)
	sub	a, -4 (ix)
	jr	Z, f_header_L00116
f_header_L00203:
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0090
    reference r164, f_header, 1, f_fail, f_fail, 0
	call	_fail
	jr	f_header_L00119
f_header_L00116:
; else record->count=(uint16_t)bytes[0]|((uint16_t)bytes[1]<<8);
	ld	a, -3 (ix)
	add	a, #0x04
	ld	-9 (ix), a
	ld	a, -2 (ix)
	adc	a, #0x00
	ld	-8 (ix), a
    reference r165, f_header, 1, sf_data, _bytes, 0
	ld	a, (#_bytes + 0)
	ld	-2 (ix), a
	ld	-7 (ix), a
	ld	-6 (ix), #0x00
    reference r166, f_header, 1, sf_data, _bytes+1, 0
	ld	a, (#_bytes + 1)
	ld	-2 (ix), a
	ld	-3 (ix), a
	ld	-2 (ix), #0x00
	ld	a, -3 (ix)
	ld	-4 (ix), a
	ld	-5 (ix), #0x00
	ld	a, -7 (ix)
	ld	-3 (ix), a
	ld	a, -4 (ix)
	ld	-2 (ix), a
	pop	bc
	pop	hl
	push	hl
	push	bc
	ld	a, -3 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -2 (ix)
	ld	(hl), a
f_header_L00119:
; return !job->error;
    reference r167, f_header, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	-3 (ix), l
	ld	-2 (ix), h
	ld	de, #0x0016
	add	hl, de
	ld	a, (hl)
	ld	-3 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-2 (ix), a
	or	a, -3 (ix)
	sub	a, #0x01
	ld	a, #0x00
	rla
	ld	-2 (ix), a
f_header_L00120:
; }
	ld	sp, ix
	pop	ix
	ret
; static uint8_t access(uint8_t writing) {
endfragment f_header

fragment f_access
; Function access
_access:
    reference r168, f_access, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
	push	af
	ld	c, a
; if(!(record->state&(writing?WRITABLE:READABLE))) {
    reference r169, f_access, 1, sf_data, _record, 0
	ld	hl, (_record)
	ex	(sp), hl
	pop	de
	push	de
	ld	hl, #16
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, c
	or	a, a
	jr	Z, f_access_L00105
	ld	hl, #0x0004
	jr	f_access_L00106
f_access_L00105:
	ld	hl, #0x0002
f_access_L00106:
	ld	a, e
	ld	b, d
	and	a, l
	ld	-2 (ix), a
	ld	a, b
	and	a, h
	ld	-1 (ix), a
	xor	a, a
	or	a, -2 (ix)
	jr	NZ, f_access_L00102
; fail(record->family==2&&!(record->state&OPENED)?4:writing?3:2,0); return 0;
	pop	hl
	push	hl
	push	bc
	ld	bc, #0x000e
	add	hl, bc
	pop	bc
	ld	b, (hl)
	inc	hl
	ld	l, (hl)
	ld	a, b
	sub	a, #0x02
	or	a, l
	jr	NZ, f_access_L00107
	ld	a, e
	and	a,#0x01
	jr	NZ, f_access_L00107
	ld	l, #0x04
	jr	f_access_L00108
f_access_L00107:
	ld	a, c
	or	a, a
	ld	l, #0x03
	jr	NZ, f_access_L00113
	ld	l, #0x02
f_access_L00113:
f_access_L00108:
	xor	a, a
	ld	h, a
	push	af
	inc	sp
    reference r170, f_access, 1, f_fail, f_fail, 0
	call	_fail
	xor	a, a
	jr	f_access_L00103
f_access_L00102:
; return 1;
	ld	a, #0x01
f_access_L00103:
; }
	ld	sp, ix
	pop	ix
	ret
; static void text_position(void) {
endfragment f_access

fragment f_text_position
; Function text_position
_text_position:
    reference r171, f_text_position, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
	push	af
; native_position=(uint32_t)record->physical_low|((uint32_t)record->physical_high<<16);
    reference r172, f_text_position, 2, sf_data, _record, 0
	ld	bc, (_record)
	ld	e, c
	ld	d, b
	ld	hl, #20
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	inc	sp
	inc	sp
	push	de
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
	ld	hl, #22
	add	hl, bc
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, -4 (ix)
    reference r173, f_text_position, 1, sf_data, _native_position, 0
	ld	(_native_position+0), a
	ld	a, -3 (ix)
    reference r174, f_text_position, 1, sf_data, _native_position+1, 0
	ld	(_native_position+1), a
    reference r175, f_text_position, 2, sf_data, _native_position+2, 0
	ld	(_native_position + 2), de
; }
	ld	sp, ix
	pop	ix
	ret
; static void text_remember(void) {
endfragment f_text_position

fragment f_text_remember
; Function text_remember
_text_remember:
; record->physical_low=(uint16_t)native_position;
    reference r176, f_text_remember, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0014
	add	hl, bc
    reference r177, f_text_remember, 2, sf_data, _native_position, 0
	ld	bc, (_native_position)
	ld	(hl), c
	inc	hl
	ld	(hl), b
; record->physical_high=(uint16_t)(native_position>>16);
    reference r178, f_text_remember, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0016
	add	hl, bc
    reference r179, f_text_remember, 2, sf_data, _native_position+2, 0
	ld	bc, (_native_position + 2)
	ld	(hl), c
	inc	hl
	ld	(hl), b
; }
	ret
; static void text_bytes(uint16_t count) {
endfragment f_text_remember

fragment f_text_bytes
; Function text_bytes
_text_bytes:
	push	ix
	ld	ix,#0
	add	ix,sp
	push	af
	push	af
	push	af
	ld	-2 (ix), l
	ld	-1 (ix), h
; uint16_t done=0,size;
	ld	bc, #0x0000
; while(done<count&&!job->error) {
f_text_bytes_L00109:
	ld	a, c
	sub	a, -2 (ix)
	ld	a, b
	sbc	a, -1 (ix)
	jr	NC, f_text_bytes_L00112
    reference r180, f_text_bytes, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0017
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_text_bytes_L00112
; size=count-done; error=write_bytes(bytes+done,&size);
	ld	a, -2 (ix)
	sub	a, c
	ld	-4 (ix), a
	ld	a, -1 (ix)
	sbc	a, b
	ld	-3 (ix), a
	ld	a, -4 (ix)
	ld	-6 (ix), a
	ld	a, -3 (ix)
	ld	-5 (ix), a
	ld	hl, #0
	add	hl, sp
	ex	de, hl
    reference r181, f_text_bytes, 1, sf_data, _bytes, 0
	ld	hl, #_bytes
	add	hl, bc
	push	bc
    reference r182, f_text_bytes, 1, f_write_bytes, f_write_bytes, 0
	call	_write_bytes
	pop	bc
	ld	e, a
; if(error) native_error(error,1);
	or	a, a
	jr	Z, f_text_bytes_L00106
	push	bc
	ld	l, #0x01
	ld	a, e
    reference r183, f_text_bytes, 1, f_native_error, f_native_error, 0
	call	_native_error
	pop	bc
	jr	f_text_bytes_L00109
f_text_bytes_L00106:
; else if(!size||size>count-done) fail(0xf0,0);
	ld	a, -5 (ix)
	or	a, -6 (ix)
	jr	Z, f_text_bytes_L00101
	ld	a, -4 (ix)
	sub	a, -6 (ix)
	ld	a, -3 (ix)
	sbc	a, -5 (ix)
	jr	NC, f_text_bytes_L00102
f_text_bytes_L00101:
	push	bc
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x00f0
    reference r184, f_text_bytes, 1, f_fail, f_fail, 0
	call	_fail
	pop	bc
	jr	f_text_bytes_L00109
f_text_bytes_L00102:
; else done+=size;
	ld	a, c
	add	a, -6 (ix)
	ld	c, a
	ld	a, b
	adc	a, -5 (ix)
	ld	b, a
	jr	f_text_bytes_L00109
f_text_bytes_L00112:
; }
	ld	sp, ix
	pop	ix
	ret
; static uint8_t disk_open(uint8_t writing) {
endfragment f_text_bytes

fragment f_disk_open
; Function disk_open
_disk_open:
	ld	c, a
; if(native_fd>=0)return 1;
    reference r185, f_disk_open, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
	bit	7, a
	jr	NZ, f_disk_open_L00102
	ld	a, #0x01
	ret
f_disk_open_L00102:
; native_fd=open_file(writing?O_WRONLY:O_RDONLY);
	ld	a, c
	or	a, a
	jr	Z, f_disk_open_L00109
	ld	b, #0x01
	jr	f_disk_open_L00110
f_disk_open_L00109:
	ld	b, #0x00
f_disk_open_L00110:
	push	bc
	ld	a, b
    reference r186, f_disk_open, 1, f_open_file, f_open_file, 0
	call	_open_file
	pop	bc
; if(native_fd<0) {native_error((uint8_t)-native_fd,writing);return 0;}
    reference r187, f_disk_open, 1, sf_data, _native_fd, 0
	ld	(_native_fd+0), a
	bit	7, a
	jr	Z, f_disk_open_L00104
    reference r188, f_disk_open, 1, sf_data, _native_fd, 0
	ld	hl, #_native_fd
	xor	a, a
	sub	a, (hl)
	ld	b, a
	ld	l, c
	ld	a, b
    reference r189, f_disk_open, 1, f_native_error, f_native_error, 0
	call	_native_error
	xor	a, a
	ret
f_disk_open_L00104:
; if(!record->family) {text_position();return seek_file(native_position);}
    reference r190, f_disk_open, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x000f
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_disk_open_L00106
    reference r191, f_disk_open, 1, f_text_position, f_text_position, 0
	call	_text_position
    reference r192, f_disk_open, 2, sf_data, _native_position, 0
	ld	de, (_native_position)
    reference r193, f_disk_open, 1, sf_data, _native_position+2, 0
	ld	hl, (_native_position + 2)
    reference r194, f_disk_open, 1, f_seek_file, f_seek_file, 0
	jp	_seek_file
f_disk_open_L00106:
; return 1;
	ld	a, #0x01
; }
	ret
; static void disk_release(void) {
endfragment f_disk_open

fragment f_disk_release
; Function disk_release
_disk_release:
; if(native_fd<0)return;
    reference r195, f_disk_release, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
	bit	7, a
	ret	NZ
; error=close_file((uint8_t)native_fd);native_fd=-1;
    reference r196, f_disk_release, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
    reference r197, f_disk_release, 1, f_close_file, f_close_file, 0
	call	_close_file
    reference r198, f_disk_release, 1, sf_data, _native_fd, 0
	ld	hl, #_native_fd
	ld	(hl), #0xff
; if(error)fail(error==ERR_NO_SUCH_ENTRY?0xff:error==ERR_CANNOT_REGISTER_MORE?0xf3:0xf0,error);
	or	a, a
	ret	Z
	cp	a, #0x04
	jr	NZ, f_disk_release_L00107
	ld	l, #0xff
	jr	f_disk_release_L00108
f_disk_release_L00107:
	cp	a, #0x14
	ld	l, #0xf3
	jr	Z, f_disk_release_L00110
	ld	l, #0xf0
f_disk_release_L00110:
f_disk_release_L00108:
	ld	h, #0x00
	push	af
	inc	sp
    reference r199, f_disk_release, 1, f_fail, f_fail, 0
	call	_fail
; }
	ret
; static uint8_t serial_open(void) {
endfragment f_disk_release

fragment f_serial_open
; Function serial_open
_serial_open:
; if(native_fd>=0) return 1;
    reference r200, f_serial_open, 1, sf_data, _native_fd, 0
	ld	a, (_native_fd+0)
	bit	7, a
	jr	NZ, f_serial_open_L00102
	ld	a, #0x01
	ret
f_serial_open_L00102:
; fd=open_path("#SER0",O_RDWR);
	ld	a, #0x02
	push	af
	inc	sp
    reference r201, f_serial_open, 1, f_serial_open, ___str_0, 0
	ld	hl, #___str_0
    reference r202, f_serial_open, 1, f_open_path, f_open_path, 0
	call	_open_path
	ld	c, a
; if(fd<0) { fail(-fd==ERR_CANNOT_REGISTER_MORE?0xf3:0x20,(uint8_t)-fd); return 0; }
	bit	7, c
	jr	Z, f_serial_open_L00104
	xor	a, a
	sub	a, c
	ld	b, a
	ld	a, c
	rlca
	sbc	a, a
	ld	e, a
	xor	a, a
	sub	a, c
	ld	c, a
	sbc	a, a
	sub	a, e
	ld	e, a
	ld	a, c
	sub	a, #0x14
	or	a, e
	ld	l, #0xf3
	jr	Z, f_serial_open_L00108
	ld	l, #0x20
f_serial_open_L00108:
	ld	h, #0x00
	push	bc
	inc	sp
    reference r203, f_serial_open, 1, f_fail, f_fail, 0
	call	_fail
	xor	a, a
	ret
f_serial_open_L00104:
; native_fd=(uint8_t)fd; return 1;
    reference r204, f_serial_open, 1, sf_data, _native_fd, 0
	ld	hl, #_native_fd
	ld	(hl), c
	ld	a, #0x01
; }
	ret
___str_0:
	.ascii "#SER0"
	.db 0x00
; static uint8_t text_peek(void) {
endfragment f_serial_open
