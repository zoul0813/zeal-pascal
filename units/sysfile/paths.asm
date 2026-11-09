; Path canonicalization
; Maintained Z80 assembly. Initially migrated from the verified native backend.
; File semantics remain specified by src/runtime/files/operations.c and its tests.

fragment f_resolve
; Function resolve
_resolve:
	push	ix
	ld	ix,#0
	add	ix,sp
	ld	iy, #-144
	add	iy, sp
	ld	sp, iy
	ld	-8 (ix), l
	ld	-7 (ix), h
	ld	-10 (ix), e
	ld	-9 (ix), d
; uint16_t i,a,n,w=3,j;
	ld	-6 (ix), #0x03
	ld	-5 (ix), #0
; n=(uint16_t)strlen(name);
	ld	l, -8 (ix)
	ld	h, -7 (ix)
    reference r723, f_resolve, 1, f_strlen, f_strlen, 0
	call	_strlen
	ld	-14 (ix), e
; if(!n||n>127||(!folder&&name[n-1]=='/')) return 0;
	ld	-13 (ix), d
	ld	a, d
	or	a, -14 (ix)
	jr	Z, P__P_1_00101
	ld	a, -14 (ix)
	ld	-12 (ix), a
	ld	a, -13 (ix)
	ld	-11 (ix), a
	ld	a, #0x7f
	cp	a, -12 (ix)
	ld	a, #0x00
	sbc	a, -11 (ix)
	jr	C, P__P_1_00101
	ld	a, 6 (ix)
	or	a, a
	jr	NZ, P__P_1_00102
	ld	a, -12 (ix)
	add	a, #0xff
	ld	-4 (ix), a
	ld	a, #0x00
	adc	a, #0xff
	ld	-3 (ix), a
	ld	a, -8 (ix)
	add	a, -4 (ix)
	ld	-2 (ix), a
	ld	a, -7 (ix)
	adc	a, -3 (ix)
	ld	-1 (ix), a
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	ld	a, (hl)
	sub	a, #0x2f
	jr	NZ, P__P_1_00102
P__P_1_00101:
	xor	a, a
    reference r724, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00102:
; if(n>1&&name[1]==':') {
	ld	a, #0x01
	cp	a, -12 (ix)
	ld	a, #0x00
	sbc	a, -11 (ix)
	jr	NC, P__P_1_00124
	ld	l, -8 (ix)
	ld	h, -7 (ix)
	inc	hl
	ld	a, (hl)
	sub	a, #0x3a
	jr	NZ, P__P_1_00124
; if(n<3||name[2]!='/'||(!folder&&n<4)) return 0;
	ld	a, -12 (ix)
	sub	a, #0x03
	ld	a, #0x00
	sbc	a, #0x00
	jr	C, P__P_1_00106
	ld	a, -8 (ix)
	ld	-2 (ix), a
	ld	a, -7 (ix)
	ld	-1 (ix), a
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	inc	hl
	inc	hl
	ld	a, (hl)
	sub	a, #0x2f
	jr	NZ, P__P_1_00106
	ld	a, 6 (ix)
	or	a, a
	jr	NZ, P__P_1_00107
	ld	a, -12 (ix)
	sub	a, #0x04
	ld	a, #0x00
	sbc	a, #0x00
	jr	NC, P__P_1_00107
P__P_1_00106:
	xor	a, a
    reference r725, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00107:
; strcpy(full,name);
	ex	de, hl
	ld	hl, #0
	add	hl, sp
	ex	de, hl
	ld	l, -8 (ix)
	ld	h, -7 (ix)
	xor	a, a
P__P_1_00554:
	cp	a, (hl)
	ldi
	jr	NZ, P__P_1_00554
    reference r726, f_resolve, 1, f_resolve, P__P_1_00125, 0
	jp	P__P_1_00125
P__P_1_00124:
; i=(uint16_t)strlen(directory);
	ld	l, -10 (ix)
	ld	h, -9 (ix)
    reference r727, f_resolve, 1, f_strlen, f_strlen, 0
	call	_strlen
	ld	-2 (ix), e
	ld	-1 (ix), d
	ld	a, -2 (ix)
	ld	-4 (ix), a
	ld	a, -1 (ix)
	ld	-3 (ix), a
; if(i<3||i>127) return 0;
	ld	a, -4 (ix)
	ld	-2 (ix), a
	ld	a, -3 (ix)
	ld	-1 (ix), a
	ld	a, -2 (ix)
	sub	a, #0x03
	ld	a, -1 (ix)
	sbc	a, #0x00
	jr	C, P__P_1_00111
	ld	a, #0x7f
	cp	a, -2 (ix)
	ld	a, #0x00
	sbc	a, -1 (ix)
	jr	NC, P__P_1_00112
P__P_1_00111:
	xor	a, a
    reference r728, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00112:
; if(name[0]=='/') { memcpy(full,directory,2); full[2]=0; }
	ld	l, -8 (ix)
	ld	h, -7 (ix)
	ld	a, (hl)
	sub	a, #0x2f
	jr	NZ, P__P_1_00119
	ld	a, -10 (ix)
	ld	-2 (ix), a
	ld	a, -9 (ix)
	ld	-1 (ix), a
	ex	de, hl
	ld	hl, #0
	add	hl, sp
	ex	de, hl
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	ldi
	ld	a, (hl)
	ld	(de), a
	ld	iy, #2
	add	iy, sp
	ld	0 (iy), #0x00
	jr	P__P_1_00120
P__P_1_00119:
; strcpy(full,directory);
	ld	hl, #0
	add	hl, sp
	ex	de, hl
	ld	l, -10 (ix)
	ld	h, -9 (ix)
	xor	a, a
P__P_1_00557:
	cp	a, (hl)
	ldi
	jr	NZ, P__P_1_00557
; if(full[i-1]!='/') { if(i==127) return 0; full[i++]='/'; full[i]=0; }
	ld	c, -2 (ix)
	ld	b, -1 (ix)
	dec	bc
	ld	hl, #0
	add	hl, sp
	add	hl, bc
	ld	a, (hl)
	sub	a, #0x2f
	jr	Z, P__P_1_00120
	ld	a, -2 (ix)
	sub	a, #0x7f
	or	a, -1 (ix)
	jr	NZ, P__P_1_00115
	xor	a, a
    reference r729, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00115:
	ld	e, -4 (ix)
	ld	d, -3 (ix)
	ld	c, -4 (ix)
	ld	b, -3 (ix)
	inc	bc
	ld	hl, #0
	add	hl, sp
	add	hl, de
	ld	(hl), #0x2f
	ld	hl, #0
	add	hl, sp
	add	hl, bc
	ld	(hl), #0x00
P__P_1_00120:
; if(strlen(full)+n>127) return 0;
	ld	hl, #0
	add	hl, sp
    reference r730, f_resolve, 1, f_strlen, f_strlen, 0
	call	_strlen
	ld	-2 (ix), e
	ld	-1 (ix), d
	ld	a, -2 (ix)
	ld	-4 (ix), a
	ld	a, -1 (ix)
	ld	-3 (ix), a
	ld	a, -4 (ix)
	add	a, -14 (ix)
	ld	-2 (ix), a
	ld	a, -3 (ix)
	adc	a, -13 (ix)
	ld	-1 (ix), a
	ld	a, #0x7f
	cp	a, -2 (ix)
	ld	a, #0x00
	sbc	a, -1 (ix)
	jr	NC, P__P_1_00122
	xor	a, a
    reference r731, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00122:
; strcat(full,name);
	ld	e, -8 (ix)
	ld	d, -7 (ix)
	ld	hl, #0
	add	hl, sp
    reference r732, f_resolve, 1, f_strcat, f_strcat, 0
	call	_strcat
P__P_1_00125:
; if(full[0]>='a'&&full[0]<='z') full[0]-=32;
	ld	iy, #0
	add	iy, sp
	ld	c, 0 (iy)
	ld	a, c
	sub	a, #0x61
	jr	C, P__P_1_00128
	ld	a, #0x7a
	sub	a, c
	jr	C, P__P_1_00128
	ld	a, c
	add	a, #0xe0
	ld	0 (iy), a
P__P_1_00128:
; if(full[0]<'A'||full[0]>'Z'||full[1]!=':'||full[2]!='/') return 0;
	ld	iy, #0
	add	iy, sp
	ld	a, 0 (iy)
	sub	a, #0x41
	jr	C, P__P_1_00130
	ld	c, 0 (iy)
	ld	a, #0x5a
	sub	a, c
	jr	C, P__P_1_00130
	ld	hl,#0x1
	add	hl,sp
	ld	a, (hl)
	sub	a, #0x3a
	jr	NZ, P__P_1_00130
	ld	a, 2 (iy)
	inc	iy
	inc	iy
	sub	a, #0x2f
	jr	Z, P__P_1_00131
P__P_1_00130:
	xor	a, a
    reference r733, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00131:
; memcpy(out,full,3); i=3;
	ld	e, 4 (ix)
	ld	d, 5 (ix)
	ld	hl, #0
	add	hl, sp
	ld	bc, #0x0003
	ldir
	ld	-16 (ix), #0x03
	ld	-15 (ix), #0
; while(full[i]) {
P__P_1_00174:
	ld	e, -16 (ix)
	ld	d, -15 (ix)
	ld	hl, #0
	add	hl, sp
	add	hl, de
	ld	a, (hl)
	or	a, a
    reference r734, f_resolve, 1, f_resolve, P__P_1_00176, 0
	jp	Z, P__P_1_00176
; while(full[i]=='/') ++i;
	ld	a, -16 (ix)
	ld	-2 (ix), a
	ld	a, -15 (ix)
	ld	-1 (ix), a
P__P_1_00135:
	ld	e, -2 (ix)
	ld	d, -1 (ix)
	ld	hl, #0
	add	hl, sp
	add	hl, de
	ld	-4 (ix), l
	ld	-3 (ix), h
	ld	a, (hl)
	sub	a, #0x2f
	jr	NZ, P__P_1_00137
	inc	-2 (ix)
	jr	NZ, P__P_1_00135
	inc	-1 (ix)
	jr	P__P_1_00135
P__P_1_00137:
; a=i; while(full[i]&&full[i]!='/') ++i; n=i-a;
	ld	a, -2 (ix)
	ld	-14 (ix), a
	ld	a, -1 (ix)
	ld	-13 (ix), a
P__P_1_00139:
	ld	e, -2 (ix)
	ld	d, -1 (ix)
	ld	hl, #0
	add	hl, sp
	add	hl, de
	ld	-4 (ix), l
	ld	-3 (ix), h
	ld	a, (hl)
	or	a, a
	jr	Z, P__P_1_00240
	sub	a, #0x2f
	jr	Z, P__P_1_00240
	inc	-2 (ix)
	jr	NZ, P__P_1_00139
	inc	-1 (ix)
	jr	P__P_1_00139
P__P_1_00240:
	ld	a, -2 (ix)
	ld	-16 (ix), a
	ld	a, -1 (ix)
	ld	-15 (ix), a
	ld	a, -2 (ix)
	sub	a, -14 (ix)
	ld	-12 (ix), a
	ld	a, -1 (ix)
	sbc	a, -13 (ix)
; if(!n) break;
	ld	-11 (ix), a
	or	a, -12 (ix)
    reference r735, f_resolve, 1, f_resolve, P__P_1_00176, 0
	jp	Z, P__P_1_00176
; if(n==1&&full[a]=='.') { if(!folder&&!full[i]) return 0; continue; }
	ld	c, -12 (ix)
	ld	b, -11 (ix)
	ld	a, c
	dec	a
	or	a, b
	jr	NZ, P__P_1_00148
	ld	e, -14 (ix)
	ld	d, -13 (ix)
	ld	hl, #0
	add	hl, sp
	add	hl, de
	ld	a, (hl)
	sub	a, #0x2e
	jr	NZ, P__P_1_00148
	ld	a, 6 (ix)
	or	a, a
    reference r736, f_resolve, 1, f_resolve, P__P_1_00174, 0
	jp	NZ, P__P_1_00174
	ld	e, -2 (ix)
	ld	d, -1 (ix)
	ld	hl, #0
	add	hl, sp
	add	hl, de
	ld	a, (hl)
	ld	-1 (ix), a
	or	a, a
    reference r737, f_resolve, 1, f_resolve, P__P_1_00174, 0
	jp	NZ, P__P_1_00174
	xor	a, a
    reference r738, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00148:
; if(n==2&&full[a]=='.'&&full[a+1]=='.') {
	ld	a, c
	sub	a, #0x02
	or	a, b
    reference r739, f_resolve, 1, f_resolve, P__P_1_00160, 0
	jp	NZ,P__P_1_00160
	ld	e, -14 (ix)
	ld	d, -13 (ix)
	ld	hl, #0
	add	hl, sp
	add	hl, de
	ld	a, (hl)
	sub	a, #0x2e
    reference r740, f_resolve, 1, f_resolve, P__P_1_00160, 0
	jp	NZ,P__P_1_00160
	ld	e, -14 (ix)
	ld	d, -13 (ix)
	inc	de
	ld	hl, #0
	add	hl, sp
	add	hl, de
	ld	a, (hl)
	sub	a, #0x2e
    reference r741, f_resolve, 1, f_resolve, P__P_1_00160, 0
	jp	NZ,P__P_1_00160
; if(!folder&&!full[i]) return 0;
	ld	a, 6 (ix)
	or	a, a
	jr	NZ, P__P_1_00151
	ld	e, -16 (ix)
	ld	d, -15 (ix)
	ld	hl, #0
	add	hl, sp
	add	hl, de
	ld	a, (hl)
	or	a, a
	jr	NZ, P__P_1_00151
	xor	a, a
    reference r742, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00151:
; if(w>3) { --w; while(w>3&&out[w-1]!='/') --w; }
	ld	c, -6 (ix)
	ld	b, -5 (ix)
	ld	a, #0x03
	cp	a, c
	ld	a, #0x00
	sbc	a, b
    reference r743, f_resolve, 1, f_resolve, P__P_1_00174, 0
	jp	NC, P__P_1_00174
	ld	l, -6 (ix)
	ld	h, -5 (ix)
	dec	hl
	ld	-6 (ix), l
	ld	-5 (ix), h
	ld	a, -6 (ix)
	ld	-2 (ix), a
	ld	a, -5 (ix)
	ld	-1 (ix), a
P__P_1_00154:
	ld	a, -2 (ix)
	ld	-4 (ix), a
	ld	a, -1 (ix)
	ld	-3 (ix), a
	ld	a, #0x03
	cp	a, -4 (ix)
	ld	a, #0x00
	sbc	a, -3 (ix)
    reference r744, f_resolve, 1, f_resolve, P__P_1_00174, 0
	jp	NC, P__P_1_00174
	ld	c, -4 (ix)
	ld	b, -3 (ix)
	dec	bc
	ld	l, 4 (ix)
	ld	h, 5 (ix)
	add	hl, bc
	ld	a, (hl)
	sub	a, #0x2f
    reference r745, f_resolve, 1, f_resolve, P__P_1_00174, 0
	jp	Z,P__P_1_00174
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	dec	hl
	ld	-2 (ix), l
	ld	-1 (ix), h
	ld	a, -2 (ix)
	ld	-6 (ix), a
	ld	a, -1 (ix)
	ld	-5 (ix), a
	jr	P__P_1_00154
; continue;
P__P_1_00160:
; if(n>15) return 0;
	ld	a, #0x0f
	cp	a, c
	ld	a, #0x00
	sbc	a, b
	jr	NC, P__P_1_00164
	xor	a, a
    reference r746, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00164:
; if(w>3&&out[w-1]!='/') out[w++]='/';
	ld	c, -6 (ix)
	ld	b, -5 (ix)
	ld	a, #0x03
	cp	a, c
	ld	a, #0x00
	sbc	a, b
	jr	NC, P__P_1_00166
	dec	bc
	ld	l, 4 (ix)
	ld	h, 5 (ix)
	add	hl, bc
	ld	a, (hl)
	sub	a, #0x2f
	jr	Z, P__P_1_00166
	ld	c, -6 (ix)
	ld	b, -5 (ix)
	inc	-6 (ix)
	jr	NZ, P__P_1_00581
	inc	-5 (ix)
P__P_1_00581:
	ld	l, 4 (ix)
	ld	h, 5 (ix)
	add	hl, bc
	ld	(hl), #0x2f
P__P_1_00166:
; if(w+n>127) return 0;
	ld	a, -6 (ix)
	add	a, -12 (ix)
	ld	c, a
	ld	a, -5 (ix)
	adc	a, -11 (ix)
	ld	b, a
	ld	a, #0x7f
	cp	a, c
	ld	a, #0x00
	sbc	a, b
	jr	NC, P__P_1_00233
	xor	a, a
    reference r747, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
; for(j=0;j<n;++j) {
P__P_1_00233:
	ld	a, -6 (ix)
	ld	-4 (ix), a
	ld	a, -5 (ix)
	ld	-3 (ix), a
	xor	a, a
	ld	-2 (ix), a
	ld	-1 (ix), a
P__P_1_00185:
	ld	a, -2 (ix)
	sub	a, -12 (ix)
	ld	a, -1 (ix)
	sbc	a, -11 (ix)
    reference r748, f_resolve, 1, f_resolve, P__P_1_00174, 0
	jp	NC, P__P_1_00174
; if((uint8_t)full[a+j]<32||full[a+j]==':') return 0;
	ld	a, -14 (ix)
	add	a, -2 (ix)
	ld	c, a
	ld	a, -13 (ix)
	adc	a, -1 (ix)
	ld	b, a
	ld	hl, #0
	add	hl, sp
	add	hl, bc
	ld	c, (hl)
	ld	a,c
	cp	a,#0x20
	jr	C, P__P_1_00170
	sub	a, #0x3a
	jr	NZ, P__P_1_00171
P__P_1_00170:
	xor	a, a
    reference r749, f_resolve, 1, f_resolve, P__P_1_00187, 0
	jp	P__P_1_00187
P__P_1_00171:
; out[w++]=full[a+j];
	ld	e, -4 (ix)
	ld	b, -3 (ix)
	inc	-4 (ix)
	jr	NZ, P__P_1_00584
	inc	-3 (ix)
P__P_1_00584:
	ld	a, -4 (ix)
	ld	-6 (ix), a
	ld	a, -3 (ix)
	ld	-5 (ix), a
	ld	a, e
	add	a, 4 (ix)
	ld	e, a
	ld	a, b
	adc	a, 5 (ix)
	ld	l, e
	ld	h, a
	ld	(hl), c
; for(j=0;j<n;++j) {
	inc	-2 (ix)
	jr	NZ, P__P_1_00185
	inc	-1 (ix)
	jr	P__P_1_00185
P__P_1_00176:
; if(!folder&&w<=3) return 0;
	ld	a, 6 (ix)
	or	a, a
	jr	NZ, P__P_1_00238
	ld	c, -6 (ix)
	ld	b, -5 (ix)
	ld	a, #0x03
	cp	a, c
	ld	a, #0x00
	sbc	a, b
	jr	C, P__P_1_00238
	xor	a, a
	jr	P__P_1_00187
; while(w>3&&out[w-1]=='/')--w;
P__P_1_00238:
	ld	a, -6 (ix)
	ld	-2 (ix), a
	ld	a, -5 (ix)
	ld	-1 (ix), a
P__P_1_00181:
	ld	c, -2 (ix)
	ld	b, -1 (ix)
	ld	a, #0x03
	cp	a, c
	ld	a, #0x00
	sbc	a, b
	jr	NC, P__P_1_00183
	dec	bc
	ld	l, 4 (ix)
	ld	h, 5 (ix)
	add	hl, bc
	ld	a, (hl)
	sub	a, #0x2f
	jr	NZ, P__P_1_00183
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	dec	hl
	ld	-2 (ix), l
	ld	-1 (ix), h
	jr	P__P_1_00181
P__P_1_00183:
; out[w]=0; return 1;
	ld	a, -2 (ix)
	add	a, 4 (ix)
	ld	-4 (ix), a
	ld	a, -1 (ix)
	adc	a, 5 (ix)
	ld	-3 (ix), a
	ld	l, -4 (ix)
	ld	h, -3 (ix)
	ld	(hl), #0x00
	ld	a, #0x01
P__P_1_00187:
; }
	ld	sp, ix
	pop	ix
	pop	hl
	pop	bc
	inc	sp
	jp	(hl)
; uint8_t path_resolve(const char *name,const char *directory,char *out) PATH_CALL {
endfragment f_resolve

fragment f_path_resolve
; Function path_resolve
_path_resolve:
	ld	c, l
; return resolve(name,directory,out,0);
	xor	a, a
	push	af
	inc	sp
	push	hl
	ld	iy, #5
	add	iy, sp
	ld	a, 0 (iy)
	ld	h, 1 (iy)
	ld	l, a
	ex	(sp), hl
	ld	l, c
    reference r750, f_path_resolve, 1, f_resolve, f_resolve, 0
	call	_resolve
; }
	pop	hl
	pop	bc
	jp	(hl)
; uint8_t path_directory(const char *name,const char *directory,char *out) PATH_CALL {
endfragment f_path_resolve

fragment f_path_directory
; Function path_directory
_path_directory:
	ld	c, l
; return resolve(name,directory,out,1);
	ld	a, #0x01
	push	af
	inc	sp
	push	hl
	ld	iy, #5
	add	iy, sp
	ld	a, 0 (iy)
	ld	h, 1 (iy)
	ld	l, a
	ex	(sp), hl
	ld	l, c
    reference r751, f_path_directory, 1, f_resolve, f_resolve, 0
	call	_resolve
; }
	pop	hl
	pop	bc
	jp	(hl)
endfragment f_path_directory
