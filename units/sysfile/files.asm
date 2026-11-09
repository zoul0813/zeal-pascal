; Files routines for the compiler-private SYSFILE Unit.
; Maintained assembly migrated from the verified backend; the C implementation
; remains the legacy/host behavior oracle, not a production build input.

fragment f_activate
; Function activate
_activate:
    reference r301, f_activate, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
	ld	c, a
; if(record->family==1&&!record->width) { fail(0x90,0); return; }
    reference r302, f_activate, 1, sf_data, _record, 0
	ld	hl, (_record)
	ex	(sp), hl
	pop	de
	push	de
	ld	hl, #14
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, e
	dec	a
	or	a, b
	jr	NZ, f_activate_L00102
	pop	hl
	push	hl
	ld	de, #0x0007
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_activate_L00102
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0090
    reference r303, f_activate, 1, f_fail, f_fail, 0
	call	_fail
    reference r304, f_activate, 1, f_activate, f_activate_L00134, 0
	jp	f_activate_L00134
f_activate_L00102:
; if(record->state&OPENED) { fail(0x20,0); return; }
	pop	hl
	push	hl
	ld	de, #0x0011
	add	hl, de
	ld	a, (hl)
	dec	hl
	bit	0, (hl)
	jr	Z, f_activate_L00105
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r305, f_activate, 1, f_fail, f_fail, 0
	call	_fail
    reference r306, f_activate, 1, f_activate, f_activate_L00134, 0
	jp	f_activate_L00134
f_activate_L00105:
; error=stat_file();
	push	bc
    reference r307, f_activate, 1, f_stat_file, f_stat_file, 0
	call	_stat_file
	ld	e, a
	pop	bc
; if(error&&(!rewriting||error!=ERR_NO_SUCH_ENTRY)) { native_error(error,rewriting); return; }
	ld	a, e
	or	a, a
	jr	Z, f_activate_L00107
	ld	a, c
	or	a, a
	jr	Z, f_activate_L00106
	ld	a, e
	sub	a, #0x04
	jr	Z, f_activate_L00107
f_activate_L00106:
	ld	l, c
	ld	a, e
    reference r308, f_activate, 1, f_native_error, f_native_error, 0
	call	_native_error
    reference r309, f_activate, 1, f_activate, f_activate_L00134, 0
	jp	f_activate_L00134
f_activate_L00107:
; if(!error&&!D_ISFILE(info.s_flags)) { fail(0x20,0); return; }
	ld	a, e
	or	a, a
	jr	NZ, f_activate_L00111
    reference r310, f_activate, 1, sf_data, _info, 0
	ld	a, (#_info + 0)
	and	a, #0x01
	dec	a
	jr	Z, f_activate_L00111
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r311, f_activate, 1, f_fail, f_fail, 0
	call	_fail
    reference r312, f_activate, 1, f_activate, f_activate_L00134, 0
	jp	f_activate_L00134
f_activate_L00111:
; if(rewriting) info.s_size=0;
	ld	a, c
	or	a, a
	jr	Z, f_activate_L00114
	ld	hl, #0x0000
    reference r313, f_activate, 1, sf_data, _info+1, 0
	ld	((_info + 1)), hl
    reference r314, f_activate, 1, sf_data, _info+3, 0
	ld	((_info + 1)+2), hl
f_activate_L00114:
; flags=rewriting?O_RDWR|O_CREAT|O_TRUNC:O_RDWR;
	ld	a, c
	or	a, a
	jr	Z, f_activate_L00136
	ld	b, #0x16
	jr	f_activate_L00137
f_activate_L00136:
	ld	b, #0x02
f_activate_L00137:
; if(record->family==1&&!record->width) { fail(0x90,0); return; }
    reference r315, f_activate, 2, sf_data, _record, 0
	ld	de, (_record)
; if(!record->family) flags=rewriting?O_WRONLY|O_CREAT|O_TRUNC:O_RDONLY;
	push	de
	pop	iy
	ld	a, 14 (iy)
	ld	l, 15 (iy)
	or	a, l
	jr	NZ, f_activate_L00116
	or	a, c
	jr	Z, f_activate_L00138
	ld	b, #0x15
	jr	f_activate_L00139
f_activate_L00138:
	ld	b, #0x00
f_activate_L00139:
f_activate_L00116:
; record->state=0;
	ld	hl, #0x0010
	add	hl, de
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
; fd=open_file(flags);
	push	bc
	ld	a, b
    reference r316, f_activate, 1, f_open_file, f_open_file, 0
	call	_open_file
	ld	e, a
	pop	bc
; if(fd<0&&!rewriting&&record->family&&-fd==ERR_READ_ONLY) {
	bit	7, e
	jr	Z, f_activate_L00118
	ld	a, c
	or	a, a
	jr	NZ, f_activate_L00118
    reference r317, f_activate, 2, sf_data, _record, 0
	ld	iy, (_record)
	ld	a, 14 (iy)
	ld	d, 15 (iy)
	or	a, d
	jr	Z, f_activate_L00118
	ld	a, e
	ld	d, a
	rlca
	sbc	a, a
	ld	l, a
	xor	a, a
	sub	a, d
	ld	d, a
	sbc	a, a
	sub	a, l
	ld	l, a
	ld	a, d
	sub	a, #0x12
; flags=O_RDONLY; fd=open_file(flags);
	or	a,l
	jr	NZ, f_activate_L00118
	ld	b,a
	push	bc
	xor	a, a
    reference r318, f_activate, 1, f_open_file, f_open_file, 0
	call	_open_file
	ld	e, a
	pop	bc
f_activate_L00118:
; if(fd<0) { native_error((uint8_t)-fd,rewriting); return; }
	bit	7, e
	jr	Z, f_activate_L00123
	xor	a, a
	sub	a, e
	ld	b, a
	ld	l, c
	ld	a, b
    reference r319, f_activate, 1, f_native_error, f_native_error, 0
	call	_native_error
    reference r320, f_activate, 1, f_activate, f_activate_L00134, 0
	jp	f_activate_L00134
f_activate_L00123:
; native_fd=fd; record->position=0; record->count=0;
    reference r321, f_activate, 1, sf_data, _native_fd, 0
	ld	hl, #_native_fd
	ld	(hl), e
    reference r322, f_activate, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0008
	add	hl, de
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
    reference r323, f_activate, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0004
	add	hl, de
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
; if(record->family==1&&!record->width) { fail(0x90,0); return; }
    reference r324, f_activate, 1, sf_data, _record, 0
	ld	hl, (_record)
; record->next=record->physical_low=record->physical_high=0;
	ld	e, l
	ld	d, h
	inc	de
	ld	a, l
	add	a, #0x14
	ld	-2 (ix), a
	ld	a, h
	adc	a, #0x00
	ld	-1 (ix), a
	push	de
	ld	de, #0x0016
	add	hl, de
	pop	de
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	pop	hl
	push	hl
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
	xor	a, a
	ld	(de), a
; record->state=OPENED|(flags&O_WRONLY?WRITABLE:flags&O_RDWR?READABLE|WRITABLE:READABLE);
    reference r325, f_activate, 2, sf_data, _record, 0
	ld	de, (_record)
	ld	hl, #0x0010
	add	hl, de
	ld	d, h
	bit	0, b
	jr	Z, f_activate_L00140
	ld	e, #0x04
	jr	f_activate_L00141
f_activate_L00140:
	bit	1, b
	ld	e, #0x06
	jr	NZ, f_activate_L00143
	ld	e, #0x02
f_activate_L00143:
f_activate_L00141:
	set	0, e
	ld	b, #0x00
	ld	h, d
	ld	(hl), e
	inc	hl
	ld	(hl), b
; if(record->family==1&&!record->width) { fail(0x90,0); return; }
    reference r326, f_activate, 1, sf_data, _record, 0
	ld	hl, (_record)
	ex	(sp), hl
	pop	de
	push	de
; if(record->family==1) header(rewriting);
	ld	hl, #14
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, e
	dec	a
	or	a, d
	jr	NZ, f_activate_L00130
	ld	a, c
    reference r327, f_activate, 1, f_header, f_header, 0
	call	_header
	jr	f_activate_L00131
f_activate_L00130:
; else if(record->family==2) {
	ld	a, e
	sub	a, #0x02
	or	a, d
	jr	NZ, f_activate_L00131
; if(info.s_size/128u>65535u) fail(0xf2,0);
    reference r328, f_activate, 2, sf_data, _info+1, 0
	ld	bc, (#(_info + 1) + 0)
    reference r329, f_activate, 2, sf_data, _info+3, 0
	ld	de, (#(_info + 1) + 2)
	ld	a, #0x07
f_activate_L00311:
	srl	d
	rr	e
	rr	b
	rr	c
	dec	a
	jr	NZ, f_activate_L00311
	ld	a, #0xff
	cp	a, c
	sbc	a, b
	ld	hl, #0x0000
	sbc	hl, de
	jr	NC, f_activate_L00125
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x00f2
    reference r330, f_activate, 1, f_fail, f_fail, 0
	call	_fail
	jr	f_activate_L00131
f_activate_L00125:
; else record->count=(uint16_t)(info.s_size/128u);
	ld	a, -2 (ix)
	add	a, #0x04
	ld	l, a
	ld	a, -1 (ix)
	adc	a, #0x00
	ld	h, a
	ld	(hl), c
	inc	hl
	ld	(hl), b
f_activate_L00131:
; if(job->error) close_record();
    reference r331, f_activate, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0017
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	Z, f_activate_L00134
    reference r332, f_activate, 1, f_close_record, f_close_record, 0
	call	_close_record
f_activate_L00134:
; }
	pop	af
	pop	ix
	ret
; static void component(uint8_t writing) {
endfragment f_activate

fragment f_component
; Function component
_component:
    reference r333, f_component, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
	dec	sp
	ld	-1 (ix), a
; uint16_t size=record->width;
    reference r334, f_component, 2, sf_data, _record, 0
	ld	bc, (_record)
	push	bc
	pop	iy
	ld	a, 6 (iy)
	ld	-3 (ix), a
	ld	a, 7 (iy)
	ld	-2 (ix), a
; if(record->family!=1) { fail(0x20,0); return; }
	ld	hl, #14
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	dec	a
	or	a, b
	jr	Z, f_component_L00102
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r335, f_component, 1, f_fail, f_fail, 0
	call	_fail
    reference r336, f_component, 1, f_component, f_component_L00124, 0
	jp	f_component_L00124
f_component_L00102:
; if(!access(writing)) return;
	ld	a, -1 (ix)
    reference r337, f_component, 1, f_access, f_access, 0
	call	_access
	or	a, a
    reference r338, f_component, 1, f_component, f_component_L00124, 0
	jp	Z,f_component_L00124
; if(!transfer_span(job->data,size)) { fail(0x1ff,0); return; }
    reference r339, f_component, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 6 (iy)
	ld	h, 7 (iy)
	pop	de
	push	de
    reference r340, f_component, 1, f_readable, f_readable, 0
	call	_readable
	or	a, a
	jr	NZ, f_component_L00106
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x01ff
    reference r341, f_component, 1, f_fail, f_fail, 0
	call	_fail
    reference r342, f_component, 1, f_component, f_component_L00124, 0
	jp	f_component_L00124
f_component_L00106:
; uint16_t size=record->width;
    reference r343, f_component, 2, sf_data, _record, 0
	ld	de, (_record)
; if(!writing&&record->position>=record->count) { fail(0x99,0); return; }
	ld	iy, #0x0008
	add	iy, de
	ld	a, -1 (ix)
	or	a, a
	jr	NZ, f_component_L00108
	ld	c, 0 (iy)
	ld	b, 1 (iy)
	ld	hl, #4
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, c
	sub	a, e
	ld	a, b
	sbc	a, d
	jr	C, f_component_L00108
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0099
    reference r344, f_component, 1, f_fail, f_fail, 0
	call	_fail
    reference r345, f_component, 1, f_component, f_component_L00124, 0
	jp	f_component_L00124
f_component_L00108:
; if(writing&&record->position==65535u) { fail(0xf2,0); return; }
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_component_L00111
	ld	a, 0 (iy)
	ld	b, 1 (iy)
	and	a, b
	inc	a
	jr	NZ, f_component_L00111
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x00f2
    reference r346, f_component, 1, f_fail, f_fail, 0
	call	_fail
    reference r347, f_component, 1, f_component, f_component_L00124, 0
	jp	f_component_L00124
f_component_L00111:
; if(!disk_open(writing)||!seek_file(4ul+(uint32_t)record->position*size)) return;
	ld	a, -1 (ix)
    reference r348, f_component, 1, f_disk_open, f_disk_open, 0
	call	_disk_open
	or	a, a
    reference r349, f_component, 1, f_component, f_component_L00124, 0
	jp	Z,f_component_L00124
    reference r350, f_component, 2, sf_data, _record, 0
	ld	iy, (_record)
	ld	l, 8 (iy)
	ld	h, 9 (iy)
	pop	de
	push	de
    reference r351, f_component, 1, f_muluint2ulong, ___muluint2ulong, 0
	call	___muluint2ulong
	ld	a, e
	add	a, #0x04
	ld	e, a
	ld	a, d
	adc	a, #0x00
	ld	d, a
	jr	NC, f_component_L00213
	inc	hl
f_component_L00213:
    reference r352, f_component, 1, f_seek_file, f_seek_file, 0
	call	_seek_file
	or	a, a
    reference r353, f_component, 1, f_component, f_component_L00124, 0
	jp	Z,f_component_L00124
; if(transfer(job->data,size,writing)!=size) { if(!job->error) fail(0x99,0); return; }
    reference r354, f_component, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 6 (iy)
	ld	h, 7 (iy)
	ld	a, -1 (ix)
	push	af
	inc	sp
	ld	e, -3 (ix)
	ld	d, -2 (ix)
    reference r355, f_component, 1, f_transfer, f_transfer, 0
	call	_transfer
	pop	hl
	push	hl
	cp	a, a
	sbc	hl, de
	jr	Z, f_component_L00119
    reference r356, f_component, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0017
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_component_L00124
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0099
    reference r357, f_component, 1, f_fail, f_fail, 0
	call	_fail
	jr	f_component_L00124
f_component_L00119:
; ++record->position; job->actual=1;
    reference r358, f_component, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0008
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	dec	hl
	inc	bc
	ld	(hl), c
	inc	hl
	ld	(hl), b
    reference r359, f_component, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	bc, #0x000a
	add	hl, bc
	ld	(hl), #0x01
	inc	hl
	ld	(hl), #0x00
; if(writing) {
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_component_L00124
; uint16_t size=record->width;
    reference r360, f_component, 2, sf_data, _record, 0
	ld	de, (_record)
; if(!writing&&record->position>=record->count) { fail(0x99,0); return; }
	ld	c, e
	ld	b, d
	ld	hl, #8
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	inc	de
	inc	de
	inc	de
	inc	de
; if(record->position>record->count) record->count=record->position;
	ld	l, e
	ld	h, d
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	xor	a, a
	sbc	hl, bc
	jr	NC, f_component_L00121
	ld	a, c
	ld	(de), a
	inc	de
	ld	a, b
	ld	(de), a
f_component_L00121:
; uint16_t size=record->width;
    reference r361, f_component, 1, sf_data, _record, 0
	ld	hl, (_record)
; record->state|=DIRTY;
	ld	bc, #0x0010
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	dec	hl
	set	3, c
	ld	(hl), c
	inc	hl
	ld	(hl), b
f_component_L00124:
; }
	ld	sp, ix
	pop	ix
	ret
; static void blocks(uint8_t writing,uint8_t partial) {
endfragment f_component

fragment f_blocks
; Function blocks
_blocks:
	push	ix
	ld	ix,#0
	add	ix,sp
	push	af
	push	af
	dec	sp
	ld	-1 (ix), a
	ld	-2 (ix), l
; uint32_t size=(uint32_t)job->count*128u;
    reference r362, f_blocks, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 8 (iy)
	ld	h, 9 (iy)
	xor	a, a
	ld	b, #0x07
f_blocks_L00229:
	add	hl, hl
	adc	a, a
	djnz	f_blocks_L00229
	ex	(sp), hl
	ld	-3 (ix), a
; if(record->family!=2) { fail(0x20,0); return; }
    reference r363, f_blocks, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x000e
	add	hl, de
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	a, c
	sub	a, #0x02
	or	a, b
	jr	Z, f_blocks_L00102
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r364, f_blocks, 1, f_fail, f_fail, 0
	call	_fail
    reference r365, f_blocks, 1, f_blocks, f_blocks_L00126, 0
	jp	f_blocks_L00126
f_blocks_L00102:
; if(!access(writing)) return;
	ld	a, -1 (ix)
    reference r366, f_blocks, 1, f_access, f_access, 0
	call	_access
	or	a, a
    reference r367, f_blocks, 1, f_blocks, f_blocks_L00126, 0
	jp	Z,f_blocks_L00126
; if(size>65535u||!transfer_span(job->data,(uint16_t)size)) { fail(0x1ff,0); return; }
	ld	a, #0xff
	cp	a, -5 (ix)
	sbc	a, -4 (ix)
	ld	a, #0x00
	sbc	a, -3 (ix)
	jr	C, f_blocks_L00105
	pop	de
	push	de
    reference r368, f_blocks, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 6 (iy)
	ld	h, 7 (iy)
    reference r369, f_blocks, 1, f_readable, f_readable, 0
	call	_readable
	or	a, a
	jr	NZ, f_blocks_L00106
f_blocks_L00105:
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x01ff
    reference r370, f_blocks, 1, f_fail, f_fail, 0
	call	_fail
    reference r371, f_blocks, 1, f_blocks, f_blocks_L00126, 0
	jp	f_blocks_L00126
f_blocks_L00106:
; if((uint32_t)record->position+job->count>65535u) { fail(0xf2,0); return; }
    reference r372, f_blocks, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0008
	add	hl, de
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	ld	e, #0x00
    reference r373, f_blocks, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 8 (iy)
	ld	h, 9 (iy)
	ex	(sp), hl
	ld	-3 (ix), #0x00
	ld	a, c
	add	a, -5 (ix)
	ld	c, a
	ld	a, b
	adc	a, -4 (ix)
	ld	b, a
	jr	NC, f_blocks_L00232
	inc	e
f_blocks_L00232:
	ld	a, #0xff
	cp	a, c
	sbc	a, b
	ld	a, #0x00
	sbc	a, e
	jr	NC, f_blocks_L00109
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x00f2
    reference r374, f_blocks, 1, f_fail, f_fail, 0
	call	_fail
    reference r375, f_blocks, 1, f_blocks, f_blocks_L00126, 0
	jp	f_blocks_L00126
f_blocks_L00109:
; if(!disk_open(writing)||!seek_file((uint32_t)record->position*128u)) return;
	ld	a, -1 (ix)
    reference r376, f_blocks, 1, f_disk_open, f_disk_open, 0
	call	_disk_open
	or	a, a
    reference r377, f_blocks, 1, f_blocks, f_blocks_L00126, 0
	jp	Z,f_blocks_L00126
    reference r378, f_blocks, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0008
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	hl, #0x0000
	ld	b, #0x07
f_blocks_L00233:
	ex	de, hl
	add	hl, hl
	ex	de, hl
	adc	hl, hl
	djnz	f_blocks_L00233
    reference r379, f_blocks, 1, f_seek_file, f_seek_file, 0
	call	_seek_file
	or	a, a
; for(i=0;i<job->count;++i) {
    reference r380, f_blocks, 1, f_blocks, f_blocks_L00126, 0
	jp	Z,f_blocks_L00126
	ld	bc, #0x0000
f_blocks_L00124:
; uint32_t size=(uint32_t)job->count*128u;
    reference r381, f_blocks, 1, sf_data, _job, 0
	ld	hl, (_job)
; for(i=0;i<job->count;++i) {
	ld	-4 (ix), l
	ld	-3 (ix), h
	ld	de, #8
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, c
	sub	a, e
	ld	a, b
	sbc	a, d
    reference r382, f_blocks, 1, f_blocks, f_blocks_L00118, 0
	jp	NC, f_blocks_L00118
; if(transfer(job->data+i*128u,128,writing)!=128) break;
	ld	e, -4 (ix)
	ld	d, -3 (ix)
	ld	hl, #6
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, de
	push	bc
	ld	a, -1 (ix)
	push	af
	inc	sp
	ld	de, #0x0080
    reference r383, f_blocks, 1, f_transfer, f_transfer, 0
	call	_transfer
	pop	bc
	ld	a, e
	sub	a, #0x80
	or	a, d
	jr	NZ, f_blocks_L00118
; ++record->position; ++job->actual;
    reference r384, f_blocks, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0008
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	dec	hl
	inc	de
	ld	(hl), e
	inc	hl
	ld	(hl), d
    reference r385, f_blocks, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x000a
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	dec	hl
	inc	de
	ld	(hl), e
	inc	hl
	ld	(hl), d
; if(writing&&record->position>record->count) record->count=record->position;
	ld	a, -1 (ix)
	or	a, a
	jr	Z, f_blocks_L00125
; if(record->family!=2) { fail(0x20,0); return; }
    reference r386, f_blocks, 2, sf_data, _record, 0
	ld	de, (_record)
; if(writing&&record->position>record->count) record->count=record->position;
	ld	l, e
	ld	h, d
	push	bc
	ld	bc, #0x0008
	add	hl, bc
	pop	bc
	ld	a, (hl)
	ld	-4 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-3 (ix), a
	inc	de
	inc	de
	inc	de
	inc	de
	ld	l, e
	ld	h, d
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	sub	a, -4 (ix)
	ld	a, h
	sbc	a, -3 (ix)
	jr	NC, f_blocks_L00125
	ld	a, -4 (ix)
	ld	(de), a
	inc	de
	ld	a, -3 (ix)
	ld	(de), a
f_blocks_L00125:
; for(i=0;i<job->count;++i) {
	inc	bc
    reference r387, f_blocks, 1, f_blocks, f_blocks_L00124, 0
	jp	f_blocks_L00124
f_blocks_L00118:
; if(!partial&&job->actual!=job->count&&!job->error) fail(writing?0xf0:0x99,0);
	ld	a, -2 (ix)
	or	a, a
	jr	NZ, f_blocks_L00126
; uint32_t size=(uint32_t)job->count*128u;
    reference r388, f_blocks, 2, sf_data, _job, 0
	ld	bc, (_job)
; if(!partial&&job->actual!=job->count&&!job->error) fail(writing?0xf0:0x99,0);
	ld	e, c
	ld	d, b
	ld	hl, #10
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	l, c
	ld	h, b
	push	bc
	ld	bc, #0x0008
	add	hl, bc
	pop	bc
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	cp	a, a
	sbc	hl, de
	jr	Z, f_blocks_L00126
	ld	hl, #22
	add	hl, bc
	ld	a, (hl)
	inc	hl
	or	a, (hl)
	jr	NZ, f_blocks_L00126
	ld	a, -1 (ix)
	or	a, a
	ld	l, #0xf0
	jr	NZ, f_blocks_L00129
	ld	l, #0x99
f_blocks_L00129:
	xor	a, a
	ld	h, a
	push	af
	inc	sp
    reference r389, f_blocks, 1, f_fail, f_fail, 0
	call	_fail
f_blocks_L00126:
; }
	ld	sp, ix
	pop	ix
	ret
; static uint8_t logical_device(const char *path) {
endfragment f_blocks

fragment f_logical_device
; Function logical_device
_logical_device:
	ld	c, l
	ld	b, h
; if(!strcmp(path,"KBD:")) return ZF_STANDARD_KBD;
	push	bc
    reference r390, f_logical_device, 1, f_logical_device, ___str_1, 0
	ld	de, #___str_1
	ld	l, c
	ld	h, b
    reference r391, f_logical_device, 1, f_strcmp, f_strcmp, 0
	call	_strcmp
	pop	bc
	ld	a, d
	or	a, e
	jr	NZ, f_logical_device_L00102
	ld	a, #0x03
	ret
f_logical_device_L00102:
; if(!strcmp(path,"CON:")) return ZF_STANDARD_CON;
	push	bc
    reference r392, f_logical_device, 1, f_logical_device, ___str_2, 0
	ld	de, #___str_2
	ld	l, c
	ld	h, b
    reference r393, f_logical_device, 1, f_strcmp, f_strcmp, 0
	call	_strcmp
	pop	bc
	ld	a, d
	or	a, e
	jr	NZ, f_logical_device_L00104
	ld	a, #0x04
	ret
f_logical_device_L00104:
; if(!strcmp(path,"TRM:")) return ZF_STANDARD_TRM;
	push	bc
    reference r394, f_logical_device, 1, f_logical_device, ___str_3, 0
	ld	de, #___str_3
	ld	l, c
	ld	h, b
    reference r395, f_logical_device, 1, f_strcmp, f_strcmp, 0
	call	_strcmp
	pop	bc
	ld	a, d
	or	a, e
	jr	NZ, f_logical_device_L00106
	ld	a, #0x05
	ret
f_logical_device_L00106:
; if(!strcmp(path,"LST:")) return ZF_STANDARD_LST;
	push	bc
    reference r396, f_logical_device, 1, f_logical_device, ___str_4, 0
	ld	de, #___str_4
	ld	l, c
	ld	h, b
    reference r397, f_logical_device, 1, f_strcmp, f_strcmp, 0
	call	_strcmp
	pop	bc
	ld	a, d
	or	a, e
	jr	NZ, f_logical_device_L00108
	ld	a, #0x06
	ret
f_logical_device_L00108:
; if(!strcmp(path,"AUX:")) return ZF_STANDARD_AUX;
	push	bc
    reference r398, f_logical_device, 1, f_logical_device, ___str_5, 0
	ld	de, #___str_5
	ld	l, c
	ld	h, b
    reference r399, f_logical_device, 1, f_strcmp, f_strcmp, 0
	call	_strcmp
	pop	bc
	ld	a, d
	or	a, e
	jr	NZ, f_logical_device_L00110
	ld	a, #0x07
	ret
f_logical_device_L00110:
; if(!strcmp(path,"USR:")) return ZF_STANDARD_USR;
    reference r400, f_logical_device, 1, f_logical_device, ___str_6, 0
	ld	de, #___str_6
	ld	l, c
	ld	h, b
    reference r401, f_logical_device, 1, f_strcmp, f_strcmp, 0
	call	_strcmp
	ld	a, d
	or	a, e
; return 0;
	ld	a, #0x08
	ret	Z
	xor	a, a
; }
	ret
___str_1:
	.ascii "KBD:"
	.db 0x00
___str_2:
	.ascii "CON:"
	.db 0x00
___str_3:
	.ascii "TRM:"
	.db 0x00
___str_4:
	.ascii "LST:"
	.db 0x00
___str_5:
	.ascii "AUX:"
	.db 0x00
___str_6:
	.ascii "USR:"
	.db 0x00
; static uint8_t assigned_path(void) {
endfragment f_logical_device

fragment f_assigned_path
; Function assigned_path
_assigned_path:
    reference r402, f_assigned_path, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
; if(!readable(job->data,1)) return 0;
    reference r403, f_assigned_path, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0006
	add	hl, de
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	de, #0x0001
    reference r404, f_assigned_path, 1, f_readable, f_readable, 0
	call	_readable
	or	a,a
    reference r405, f_assigned_path, 1, f_assigned_path, f_assigned_path_L00124, 0
	jp	Z,f_assigned_path_L00124
    reference r406, f_assigned_path, 1, sf_data, _job, 0
	ld	hl, (_job)
; n=USER(job->data)[0];
	ld	de, #0x0006
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	l, e
	ld	h, d
	ld	c, (hl)
	ld	b, #0x00
; if(!n||n>127||!readable(job->data+1,n)||memchr(USER(job->data+1),0,n)) return 0;
	ld	a, b
	or	a, c
	jr	Z, f_assigned_path_L00103
	ld	l, c
	ld	a, #0x7f
	sub	a, l
	jr	C, f_assigned_path_L00103
	inc	de
	push	bc
	ex	de, hl
	ld	e, c
	ld	d, b
    reference r407, f_assigned_path, 1, f_readable, f_readable, 0
	call	_readable
	pop	bc
	or	a, a
	jr	Z, f_assigned_path_L00103
	inc	sp
	inc	sp
	push	bc
    reference r408, f_assigned_path, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 6 (iy)
	ld	h, 7 (iy)
	inc	hl
	push	bc
	ld	e, -2 (ix)
	ld	d, #0x00
	push	de
	ld	de, #0x0000
    reference r409, f_assigned_path, 1, f_memchr, f_memchr, 0
	call	_memchr
	pop	bc
	ld	a, d
	or	a, e
	jr	Z, f_assigned_path_L00104
f_assigned_path_L00103:
	xor	a, a
    reference r410, f_assigned_path, 1, f_assigned_path, f_assigned_path_L00124, 0
	jp	f_assigned_path_L00124
f_assigned_path_L00104:
; memcpy(transport.request.path,USER(job->data+1),n); transport.request.path[n]=0;
    reference r411, f_assigned_path, 1, sf_data, _transport+1, 0
	ld	de, #(_transport + 1)
    reference r412, f_assigned_path, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	l, 6 (iy)
	ld	h, 7 (iy)
	inc	hl
	push	bc
	ld	c, -2 (ix)
	ld	b, -1 (ix)
	ld	a, b
	or	a, c
	jr	Z, f_assigned_path_L00218
	ldir
f_assigned_path_L00218:
	pop	bc
    reference r413, f_assigned_path, 1, sf_data, _transport+1, 0
	ld	hl, #(_transport + 1)
	add	hl, bc
	ld	(hl), #0x00
; if(n==4&&transport.request.path[3]==':') {
	ld	a, -2 (ix)
	sub	a, #0x04
	or	a, -1 (ix)
	jr	NZ, f_assigned_path_L00115
    reference r414, f_assigned_path, 1, sf_data, _transport+4, 0
	ld	a, (#_transport + 4)
	sub	a, #0x3a
	jr	NZ, f_assigned_path_L00115
; memcpy(bytes,transport.request.path,5);
    reference r415, f_assigned_path, 1, sf_data, _bytes, 0
	ld	de, #_bytes
    reference r416, f_assigned_path, 1, sf_data, _transport+1, 0
	ld	hl, #(_transport + 1)
	ld	bc, #0x0005
	ldir
; for(n=0;n<3;++n) if(bytes[n]>='a'&&bytes[n]<='z') bytes[n]-=32;
	ld	bc, #0x0000
f_assigned_path_L00122:
    reference r417, f_assigned_path, 1, sf_data, _bytes, 0
	ld	hl, #_bytes
	add	hl, bc
	ld	e, (hl)
	ld	a, e
	sub	a, #0x61
	jr	C, f_assigned_path_L00123
	ld	a, #0x7a
	sub	a, e
	jr	C, f_assigned_path_L00123
	ld	a, e
	add	a, #0xe0
	ld	(hl), a
f_assigned_path_L00123:
	inc	bc
	ld	e, c
	ld	d, b
	ld	a, e
	sub	a, #0x03
	ld	a, d
	sbc	a, #0x00
	jr	C, f_assigned_path_L00122
; if(logical_device((char*)bytes)) { strcpy(transport.request.path,(char*)bytes); return 1; }
	push	bc
    reference r418, f_assigned_path, 1, sf_data, _bytes, 0
	ld	hl, #_bytes
    reference r419, f_assigned_path, 1, f_logical_device, f_logical_device, 0
	call	_logical_device
	pop	bc
	or	a, a
	jr	Z, f_assigned_path_L00115
    reference r420, f_assigned_path, 1, sf_data, _transport+1, 0
	ld	de, #(_transport + 1)
    reference r421, f_assigned_path, 1, sf_data, _bytes, 0
	ld	hl, #_bytes
	xor	a, a
f_assigned_path_L00223:
	cp	a, (hl)
	ldi
	jr	NZ, f_assigned_path_L00223
	ld	a, #0x01
	jr	f_assigned_path_L00124
f_assigned_path_L00115:
; bytes[0]=0;
    reference r422, f_assigned_path, 1, sf_data, _bytes, 0
	ld	de, #_bytes+0
	xor	a, a
	ld	(de), a
; if(n<2||transport.request.path[1]!=':') {
	ld	a, c
	sub	a, #0x02
	ld	a, b
	sbc	a, #0x00
	jr	C, f_assigned_path_L00119
    reference r423, f_assigned_path, 1, sf_data, _transport+2, 0
	ld	a, (#_transport + 2)
	sub	a, #0x3a
	jr	Z, f_assigned_path_L00120
f_assigned_path_L00119:
; error=curdir((char*)bytes);
	ld	l, e
	ld	h, d
	push	de
    reference r424, f_assigned_path, 1, f_curdir, f_curdir, 0
	call	_curdir
	pop	de
	ld	b, a
; if(error) { fail(0x20,error); return 0; }
	or	a, a
	jr	Z, f_assigned_path_L00120
	push	bc
	inc	sp
	ld	hl, #0x0020
    reference r425, f_assigned_path, 1, f_fail, f_fail, 0
	call	_fail
	xor	a, a
	jr	f_assigned_path_L00124
f_assigned_path_L00120:
; return path_resolve(transport.request.path,(char*)bytes,transport.request.path);
    reference r426, f_assigned_path, 1, sf_data, _transport+1, 0
	ld	hl, #(_transport + 1)
	push	hl
    reference r427, f_assigned_path, 1, f_path_resolve, f_path_resolve, 0
	call	_path_resolve
f_assigned_path_L00124:
; }
	pop	bc
	pop	ix
	ret
; static void rename_close(int8_t fd) {
endfragment f_assigned_path

fragment f_rename_close
; Function rename_close
_rename_close:
	ld	c, a
; if(fd<0) return;
	bit	7, c
	ret	NZ
; error=close_file((uint8_t)fd);
	ld	a, c
    reference r428, f_rename_close, 1, f_close_file, f_close_file, 0
	call	_close_file
	ld	c, a
; if(error) native_error(error,2);
	or	a, a
	ret	Z
	ld	l, #0x02
	ld	a, c
; }
    reference r429, f_rename_close, 1, f_native_error, f_native_error, 0
	jp	_native_error
; static void rename_read(int8_t fd,uint8_t *out,uint16_t count) {
endfragment f_rename_close

fragment f_rename_read
; Function rename_read
_rename_read:
    reference r430, f_rename_read, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	ld	hl, #-8
	add	hl, sp
	ld	sp, hl
	ld	-1 (ix), a
	ld	-3 (ix), e
	ld	-2 (ix), d
; uint16_t done=0,n;
	ld	bc, #0x0000
; while(done<count&&!job->error) {
f_rename_read_L00109:
	ld	a, c
	sub	a, 4 (ix)
	ld	a, b
	sbc	a, 5 (ix)
    reference r431, f_rename_read, 1, f_rename_read, f_rename_read_L00112, 0
	jp	NC, f_rename_read_L00112
    reference r432, f_rename_read, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0017
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_rename_read_L00112
; n=count-done; error=read(fd,out+done,&n);
	ld	a, 4 (ix)
	sub	a, c
	ld	-6 (ix), a
	ld	a, 5 (ix)
	sbc	a, b
	ld	-5 (ix), a
	ld	a, -6 (ix)
	ld	-8 (ix), a
	ld	a, -5 (ix)
	ld	-7 (ix), a
	ld	l, -3 (ix)
	ld	h, -2 (ix)
	add	hl, bc
	push	bc
	ex	de, hl
	ld	hl, #2
	add	hl, sp
	push	hl
	ld	a, -1 (ix)
    reference r433, f_rename_read, 1, f_read, f_read, 0
	call	_read
	pop	bc
	ld	-4 (ix), a
; if(error) native_error(error,0);
	or	a, a
	jr	Z, f_rename_read_L00106
	push	bc
	ld	l, #0x00
	ld	a, -4 (ix)
    reference r434, f_rename_read, 1, f_native_error, f_native_error, 0
	call	_native_error
	pop	bc
	jr	f_rename_read_L00109
f_rename_read_L00106:
; else if(!n||n>count-done) fail(0x99,0);
	ld	a, -7 (ix)
	or	a, -8 (ix)
	jr	Z, f_rename_read_L00101
	ld	a, -6 (ix)
	sub	a, -8 (ix)
	ld	a, -5 (ix)
	sbc	a, -7 (ix)
	jr	NC, f_rename_read_L00102
f_rename_read_L00101:
	push	bc
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0099
    reference r435, f_rename_read, 1, f_fail, f_fail, 0
	call	_fail
	pop	bc
	jr	f_rename_read_L00109
f_rename_read_L00102:
; else done+=n;
	ld	a, c
	add	a, -8 (ix)
	ld	c, a
	ld	a, b
	adc	a, -7 (ix)
	ld	b, a
    reference r436, f_rename_read, 1, f_rename_read, f_rename_read_L00109, 0
	jp	f_rename_read_L00109
f_rename_read_L00112:
; }
	ld	sp, ix
	pop	ix
	pop	hl
	pop	af
	jp	(hl)
; static void rename_end(int8_t fd) {
endfragment f_rename_read

fragment f_rename_end
; Function rename_end
_rename_end:
    reference r437, f_rename_end, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	push	af
	ld	c, a
; uint16_t n=1;
	ld	hl, #0x0001
	ex	(sp), hl
; uint8_t error=read(fd,bytes,&n);
	ld	hl, #0
	add	hl, sp
	push	hl
    reference r438, f_rename_end, 1, sf_data, _bytes, 0
	ld	de, #_bytes
	ld	a, c
    reference r439, f_rename_end, 1, f_read, f_read, 0
	call	_read
	ld	c, a
; if(error) native_error(error,0);
	or	a, a
	jr	Z, f_rename_end_L00104
	ld	l, #0x00
	ld	a, c
    reference r440, f_rename_end, 1, f_native_error, f_native_error, 0
	call	_native_error
	jr	f_rename_end_L00106
f_rename_end_L00104:
; else if(n) fail(0xf0,0);
	ld	a, -1 (ix)
	or	a, -2 (ix)
	jr	Z, f_rename_end_L00106
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x00f0
    reference r441, f_rename_end, 1, f_fail, f_fail, 0
	call	_fail
f_rename_end_L00106:
; }
	pop	af
	pop	ix
	ret
; static void rename_file(void) {
endfragment f_rename_end

fragment f_rename_file
; Function rename_file
_rename_file:
    reference r442, f_rename_file, 1, f_sdcc_enter_ix, f_sdcc_enter_ix, 0
	call	___sdcc_enter_ix
	ld	hl, #-16
	add	hl, sp
	ld	sp, hl
; if(record->state&OPENED) { fail(0x20,0); return; }
    reference r443, f_rename_file, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	de, #0x0011
	add	hl, de
	ld	a, (hl)
	dec	hl
	bit	0, (hl)
	jr	Z, f_rename_file_L00102
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r444, f_rename_file, 1, f_fail, f_fail, 0
	call	_fail
    reference r445, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	f_rename_file_L00158
f_rename_file_L00102:
; if(!assigned_path()) { fail(0x20,ERR_INVALID_PATH); return; }
    reference r446, f_rename_file, 1, f_assigned_path, f_assigned_path, 0
	call	_assigned_path
	or	a, a
	jr	NZ, f_rename_file_L00104
	ld	a, #0x0b
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r447, f_rename_file, 1, f_fail, f_fail, 0
	call	_fail
    reference r448, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	f_rename_file_L00158
f_rename_file_L00104:
; if(logical_device(transport.request.path)) { fail(0x20,ERR_INVALID_PATH); return; }
    reference r449, f_rename_file, 1, sf_data, _transport+1, 0
	ld	hl, #(_transport + 1)
    reference r450, f_rename_file, 1, f_logical_device, f_logical_device, 0
	call	_logical_device
	or	a, a
	jr	Z, f_rename_file_L00106
	ld	a, #0x0b
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r451, f_rename_file, 1, f_fail, f_fail, 0
	call	_fail
    reference r452, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	f_rename_file_L00158
f_rename_file_L00106:
; strcpy(rename_path,transport.request.path);
    reference r453, f_rename_file, 1, sf_data, _rename_path, 0
	ld	de, #_rename_path
    reference r454, f_rename_file, 1, sf_data, _transport+1, 0
	ld	hl, #(_transport + 1)
	xor	a, a
f_rename_file_L00389:
	cp	a, (hl)
	ldi
	jr	NZ, f_rename_file_L00389
; error=stat(rename_path,&info);
    reference r455, f_rename_file, 1, sf_data, _info, 0
	ld	de, #_info
    reference r456, f_rename_file, 1, sf_data, _rename_path, 0
	ld	hl, #_rename_path
    reference r457, f_rename_file, 1, f_stat, f_stat, 0
	call	_stat
; if(!error) { fail(0x20,ERR_ALREADY_EXIST); return; }
	ld	c, a
	or	a, a
	jr	NZ, f_rename_file_L00108
	ld	a, #0x0f
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r458, f_rename_file, 1, f_fail, f_fail, 0
	call	_fail
    reference r459, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	f_rename_file_L00158
f_rename_file_L00108:
; if(error!=ERR_NO_SUCH_ENTRY) { native_error(error,2); return; }
	ld	a, c
	sub	a, #0x04
	jr	Z, f_rename_file_L00110
	ld	l, #0x02
	ld	a, c
    reference r460, f_rename_file, 1, f_native_error, f_native_error, 0
	call	_native_error
    reference r461, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	f_rename_file_L00158
f_rename_file_L00110:
; error=stat_file();
    reference r462, f_rename_file, 1, f_stat_file, f_stat_file, 0
	call	_stat_file
; if(error) { native_error(error,2); return; }
	ld	c, a
	or	a, a
	jr	Z, f_rename_file_L00112
	ld	l, #0x02
	ld	a, c
    reference r463, f_rename_file, 1, f_native_error, f_native_error, 0
	call	_native_error
    reference r464, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	f_rename_file_L00158
f_rename_file_L00112:
; if(!D_ISFILE(info.s_flags)) { fail(0x20,0); return; }
    reference r465, f_rename_file, 1, sf_data, _info, 0
	ld	a, (#_info + 0)
	and	a, #0x01
	dec	a
	jr	Z, f_rename_file_L00114
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x0020
    reference r466, f_rename_file, 1, f_fail, f_fail, 0
	call	_fail
    reference r467, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	f_rename_file_L00158
f_rename_file_L00114:
; size=info.s_size;
    reference r468, f_rename_file, 1, sf_data, _info+1, 0
	ld	de, #_info + 1
	ld	hl, #2
	add	hl, sp
	ex	de, hl
	ld	bc, #0x0004
	ldir
; in=open_file(O_RDONLY);
	xor	a, a
    reference r469, f_rename_file, 1, f_open_file, f_open_file, 0
	call	_open_file
	ld	-10 (ix), a
; if(in<0) { native_error((uint8_t)-in,2); return; }
	bit	7, -10 (ix)
	jr	Z, f_rename_file_L00116
	xor	a, a
	sub	a, -10 (ix)
	ld	l, #0x02
    reference r470, f_rename_file, 1, f_native_error, f_native_error, 0
	call	_native_error
    reference r471, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	f_rename_file_L00158
f_rename_file_L00116:
; out=open_path(rename_path,O_WRONLY|O_CREAT);
	ld	a, #0x11
	push	af
	inc	sp
    reference r472, f_rename_file, 1, sf_data, _rename_path, 0
	ld	hl, #_rename_path
    reference r473, f_rename_file, 1, f_open_path, f_open_path, 0
	call	_open_path
	ld	-9 (ix), a
; if(out<0) native_error((uint8_t)-out,2);
	bit	7, -9 (ix)
	jr	Z, f_rename_file_L00118
	xor	a, a
	sub	a, -9 (ix)
	ld	l, #0x02
    reference r474, f_rename_file, 1, f_native_error, f_native_error, 0
	call	_native_error
f_rename_file_L00118:
; left=size;
	ld	hl, #10
	add	hl, sp
	ex	de, hl
	ld	hl, #2
	add	hl, sp
	ld	bc, #4
	ldir
; while(left&&!job->error) {
f_rename_file_L00131:
    reference r475, f_rename_file, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0016
	add	hl, de
	ld	a, (hl)
	ld	-2 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-1 (ix), a
	ld	a, -3 (ix)
	or	a, -4 (ix)
	or	a, -5 (ix)
	or	a, -6 (ix)
    reference r476, f_rename_file, 1, f_rename_file, f_rename_file_L00133, 0
	jp	Z, f_rename_file_L00133
	ld	a, -1 (ix)
	or	a, -2 (ix)
    reference r477, f_rename_file, 1, f_rename_file, f_rename_file_L00133, 0
	jp	NZ, f_rename_file_L00133
; n=left>sizeof(bytes)?sizeof(bytes):(uint16_t)left;
	ld	a, #0x80
	cp	a, -6 (ix)
	ld	a, #0x00
	sbc	a, -5 (ix)
	ld	a, #0x00
	sbc	a, -4 (ix)
	ld	a, #0x00
	sbc	a, -3 (ix)
	jr	NC, f_rename_file_L00160
	ld	bc, #0x0080
	jr	f_rename_file_L00161
f_rename_file_L00160:
	ld	c, -6 (ix)
	ld	b, #0x00
f_rename_file_L00161:
	ld	-8 (ix), c
	ld	-7 (ix), b
; rename_read(in,bytes,n); done=0;
	ld	l, -8 (ix)
	ld	h, #0x00
	push	hl
    reference r478, f_rename_file, 1, sf_data, _bytes, 0
	ld	de, #_bytes
	ld	a, -10 (ix)
    reference r479, f_rename_file, 1, f_rename_read, f_rename_read, 0
	call	_rename_read
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
; while(done<n&&!job->error) {
f_rename_file_L00127:
	ld	a, -2 (ix)
	sub	a, -8 (ix)
	ld	a, -1 (ix)
	sbc	a, -7 (ix)
	jr	NC, f_rename_file_L00129
    reference r480, f_rename_file, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0017
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_rename_file_L00129
; written=n-done; error=write(out,bytes+done,&written);
	ld	a, -8 (ix)
	sub	a, -2 (ix)
	ld	c, a
	sbc	a, a
	sub	a, -1 (ix)
	ld	b, a
	inc	sp
	inc	sp
	push	bc
	ld	hl, #0
	add	hl, sp
    reference r481, f_rename_file, 1, sf_data, _bytes, 1
	ld	a, #<(_bytes)
	add	a, -2 (ix)
	ld	e, a
    reference r482, f_rename_file, 1, sf_data, _bytes, 2
	ld	a, #>(_bytes)
	adc	a, -1 (ix)
	ld	d, a
	push	bc
	push	hl
	ld	a, -9 (ix)
    reference r483, f_rename_file, 1, f_write, f_write, 0
	call	_write
	ld	e, a
	pop	bc
; if(error) native_error(error,1);
	ld	a, e
	or	a, a
	jr	Z, f_rename_file_L00124
	ld	l, #0x01
	ld	a, e
    reference r484, f_rename_file, 1, f_native_error, f_native_error, 0
	call	_native_error
	jr	f_rename_file_L00127
f_rename_file_L00124:
; else if(!written||written>n-done) fail(0xf0,0);
	ld	a, -15 (ix)
	or	a, -16 (ix)
	jr	Z, f_rename_file_L00119
	ld	a, c
	sub	a, -16 (ix)
	ld	a, b
	sbc	a, -15 (ix)
	jr	NC, f_rename_file_L00120
f_rename_file_L00119:
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x00f0
    reference r485, f_rename_file, 1, f_fail, f_fail, 0
	call	_fail
	jr	f_rename_file_L00127
f_rename_file_L00120:
; else done+=written;
	ld	a, -2 (ix)
	add	a, -16 (ix)
	ld	-2 (ix), a
	ld	a, -1 (ix)
	adc	a, -15 (ix)
	ld	-1 (ix), a
    reference r486, f_rename_file, 1, f_rename_file, f_rename_file_L00127, 0
	jp	f_rename_file_L00127
f_rename_file_L00129:
; left-=n;
	ld	c, -8 (ix)
	ld	b, #0x00
	ld	de, #0x0000
	ld	a, -6 (ix)
	sub	a, c
	ld	-6 (ix), a
	ld	a, -5 (ix)
	sbc	a, b
	ld	-5 (ix), a
	ld	a, -4 (ix)
	sbc	a, e
	ld	-4 (ix), a
	ld	a, -3 (ix)
	sbc	a, d
	ld	-3 (ix), a
    reference r487, f_rename_file, 1, f_rename_file, f_rename_file_L00131, 0
	jp	f_rename_file_L00131
f_rename_file_L00133:
; if(!job->error) rename_end(in);
	ld	a, -1 (ix)
	or	a, -2 (ix)
	jr	NZ, f_rename_file_L00135
	ld	a, -10 (ix)
    reference r488, f_rename_file, 1, f_rename_end, f_rename_end, 0
	call	_rename_end
f_rename_file_L00135:
; rename_close(out); rename_close(in);
	ld	a, -9 (ix)
    reference r489, f_rename_file, 1, f_rename_close, f_rename_close, 0
	call	_rename_close
	ld	a, -10 (ix)
    reference r490, f_rename_file, 1, f_rename_close, f_rename_close, 0
	call	_rename_close
; if(job->error) return;
    reference r491, f_rename_file, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0017
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
; in=open_file(O_RDONLY);
	or	a,l
    reference r492, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	NZ,f_rename_file_L00158
    reference r493, f_rename_file, 1, f_open_file, f_open_file, 0
	call	_open_file
	ld	-6 (ix), a
; if(in<0) { native_error((uint8_t)-in,2); return; }
	bit	7, -6 (ix)
	jr	Z, f_rename_file_L00139
	xor	a, a
	sub	a, -6 (ix)
	ld	l, #0x02
    reference r494, f_rename_file, 1, f_native_error, f_native_error, 0
	call	_native_error
    reference r495, f_rename_file, 1, f_rename_file, f_rename_file_L00158, 0
	jp	f_rename_file_L00158
f_rename_file_L00139:
; out=open_path(rename_path,O_RDONLY);
	xor	a, a
	push	af
	inc	sp
    reference r496, f_rename_file, 1, sf_data, _rename_path, 0
	ld	hl, #_rename_path
    reference r497, f_rename_file, 1, f_open_path, f_open_path, 0
	call	_open_path
	ld	-5 (ix), a
; if(out<0) native_error((uint8_t)-out,2);
	bit	7, -5 (ix)
	jr	Z, f_rename_file_L00141
	xor	a, a
	sub	a, -5 (ix)
	ld	-1 (ix), a
	ld	l, #0x02
    reference r498, f_rename_file, 1, f_native_error, f_native_error, 0
	call	_native_error
f_rename_file_L00141:
; left=size;
	ld	hl, #12
	add	hl, sp
	ex	de, hl
	ld	hl, #2
	add	hl, sp
	ld	bc, #4
	ldir
; while(left&&!job->error) {
f_rename_file_L00146:
; while(left&&!job->error) {
    reference r499, f_rename_file, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	-8 (ix), l
	ld	-7 (ix), h
	ld	de, #0x0016
	add	hl, de
	ld	a, (hl)
	ld	-8 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-7 (ix), a
; while(left&&!job->error) {
	ld	a, -1 (ix)
	or	a, -2 (ix)
	or	a, -3 (ix)
	or	a, -4 (ix)
    reference r500, f_rename_file, 1, f_rename_file, f_rename_file_L00148, 0
	jp	Z, f_rename_file_L00148
	ld	a, -7 (ix)
	or	a, -8 (ix)
    reference r501, f_rename_file, 1, f_rename_file, f_rename_file_L00148, 0
	jp	NZ, f_rename_file_L00148
; n=left>sizeof(bytes)?sizeof(bytes):(uint16_t)left;
	ld	a, #0x80
	cp	a, -4 (ix)
	ld	a, #0x00
	sbc	a, -3 (ix)
	ld	a, #0x00
	sbc	a, -2 (ix)
	ld	a, #0x00
	sbc	a, -1 (ix)
	jr	NC, f_rename_file_L00162
	ld	-8 (ix), #0x80
	ld	-7 (ix), #0
	jr	f_rename_file_L00163
f_rename_file_L00162:
	ld	a, -4 (ix)
	ld	-8 (ix), a
	ld	-7 (ix), #0x00
f_rename_file_L00163:
	ld	a, -8 (ix)
	ld	-10 (ix), a
	ld	-9 (ix), #0x00
; rename_read(in,bytes,n); rename_read(out,transport.verify_bytes,n);
	ld	l, -10 (ix)
	ld	h, #0x00
	push	hl
    reference r502, f_rename_file, 1, sf_data, _bytes, 0
	ld	de, #_bytes
	ld	a, -6 (ix)
    reference r503, f_rename_file, 1, f_rename_read, f_rename_read, 0
	call	_rename_read
	ld	l, -10 (ix)
	ld	h, #0x00
	push	hl
    reference r504, f_rename_file, 1, sf_data, _transport, 0
	ld	de, #_transport
	ld	a, -5 (ix)
    reference r505, f_rename_file, 1, f_rename_read, f_rename_read, 0
	call	_rename_read
; if(!job->error&&memcmp(bytes,transport.verify_bytes,n)) fail(0xf0,0);
    reference r506, f_rename_file, 2, sf_data, _job, 0
	ld	iy, (_job)
	ld	a, 22 (iy)
	ld	-8 (ix), a
	ld	a, 23 (iy)
	ld	-7 (ix), a
	or	a, -8 (ix)
	jr	NZ, f_rename_file_L00143
	ld	c, -10 (ix)
	ld	b, #0x00
	push	bc
    reference r507, f_rename_file, 1, sf_data, _transport, 0
	ld	de, #_transport
    reference r508, f_rename_file, 1, sf_data, _bytes, 0
	ld	hl, #_bytes
    reference r509, f_rename_file, 1, f_memcmp, f_memcmp, 0
	call	_memcmp
	ld	a, d
	or	a, e
	jr	Z, f_rename_file_L00143
	xor	a, a
	push	af
	inc	sp
	ld	hl, #0x00f0
    reference r510, f_rename_file, 1, f_fail, f_fail, 0
	call	_fail
f_rename_file_L00143:
; left-=n;
	ld	c, -10 (ix)
	ld	b, #0x00
	ld	de, #0x0000
	ld	a, -4 (ix)
	sub	a, c
	ld	-4 (ix), a
	ld	a, -3 (ix)
	sbc	a, b
	ld	-3 (ix), a
	ld	a, -2 (ix)
	sbc	a, e
	ld	-2 (ix), a
	ld	a, -1 (ix)
	sbc	a, d
	ld	-1 (ix), a
    reference r511, f_rename_file, 1, f_rename_file, f_rename_file_L00146, 0
	jp	f_rename_file_L00146
f_rename_file_L00148:
; if(!job->error) { rename_end(in); if(!job->error) rename_end(out); }
	ld	a, -7 (ix)
	or	a, -8 (ix)
	jr	NZ, f_rename_file_L00152
	ld	a, -6 (ix)
    reference r512, f_rename_file, 1, f_rename_end, f_rename_end, 0
	call	_rename_end
    reference r513, f_rename_file, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	-2 (ix), l
	ld	-1 (ix), h
	ld	de, #0x0016
	add	hl, de
	ld	a, (hl)
	ld	-2 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-1 (ix), a
	or	a, -2 (ix)
	jr	NZ, f_rename_file_L00152
	ld	a, -5 (ix)
    reference r514, f_rename_file, 1, f_rename_end, f_rename_end, 0
	call	_rename_end
f_rename_file_L00152:
; rename_close(out); rename_close(in);
	ld	a, -5 (ix)
    reference r515, f_rename_file, 1, f_rename_close, f_rename_close, 0
	call	_rename_close
	ld	a, -6 (ix)
    reference r516, f_rename_file, 1, f_rename_close, f_rename_close, 0
	call	_rename_close
; if(job->error) return;
    reference r517, f_rename_file, 1, sf_data, _job, 0
	ld	hl, (_job)
	ld	de, #0x0017
	add	hl, de
	ld	a, (hl)
	dec	hl
	ld	l, (hl)
	or	a, l
	jr	NZ, f_rename_file_L00158
; error=remove_file();
    reference r518, f_rename_file, 1, f_remove_file, f_remove_file, 0
	call	_remove_file
; if(error) native_error(error,2);
	ld	c, a
	or	a, a
	jr	Z, f_rename_file_L00156
	ld	l, #0x02
	ld	a, c
    reference r519, f_rename_file, 1, f_native_error, f_native_error, 0
	call	_native_error
	jr	f_rename_file_L00158
f_rename_file_L00156:
; else strcpy(record->path,rename_path);
    reference r520, f_rename_file, 1, sf_data, _record, 0
	ld	hl, (_record)
	ld	bc, #0x0030
	add	hl, bc
    reference r521, f_rename_file, 1, sf_data, _rename_path, 0
	ld	de, #_rename_path
	ex	de, hl
	xor	a, a
f_rename_file_L00392:
	cp	a, (hl)
	ldi
	jr	NZ, f_rename_file_L00392
f_rename_file_L00158:
; }
	ld	sp, ix
	pop	ix
	ret
; static void serial_stream(void) {
endfragment f_rename_file
