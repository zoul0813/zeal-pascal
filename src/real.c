#include "real.h"

/* Real6 is sign * M * 2^(E-168), with a normalized 40-bit M.
 * Six-byte working magnitudes keep the extra carry needed by add/divide.
 * All discarded arithmetic bits are truncated, never IEEE rounded. */
uint8_t zr_op,zr_error,zr_a[6],zr_b[6],zr_result[6];
int16_t zr_integer;
const uint8_t *zr_text;
uint16_t zr_length;
static uint8_t a[6],b[6],work[10],negative_a,negative_b;
static int16_t exponent_a,exponent_b;
static uint8_t decimal_value[6],decimal_scale[6];
/* Exact 10^(4*k), rounded to the nearest Real6. Mathematical constants, not
 * compiler-folded floating expressions. Scaling then uses truncated arithmetic. */
static const uint8_t powers[10][6]={
    {129,0,0,0,0,0},{142,0,0,0,64,28},{155,0,0,32,188,62},
    {168,0,16,165,212,104},{182,4,191,201,27,14},{195,172,197,235,120,45},
    {208,205,206,27,194,83},{222,249,120,57,63,1},
    {235,43,168,173,197,29},{248,201,123,206,151,64}
};

static void clear(uint8_t *p,uint8_t count) {
    while(count--) *p++=0;
}
static void copy(uint8_t *to,const uint8_t *from,uint8_t count) {
    while(count--) *to++=*from++;
}
static uint8_t nonzero(const uint8_t *p) {
    uint8_t i,value=0;
    for(i=0;i<6;++i) value|=p[i];
    return value!=0;
}
static void left(uint8_t *p) {
    uint8_t i,carry=0,next;
    for(i=0;i<6;++i) {
        next=p[i]>>7; p[i]=(uint8_t)((p[i]<<1)|carry); carry=next;
    }
}
static void right(uint8_t *p,uint16_t count) {
    uint8_t i,carry,next;
    if(count>=48) { clear(p,6); return; }
    while(count--) {
        carry=0; i=6;
        while(i--) { next=p[i]&1; p[i]=(uint8_t)((p[i]>>1)|(carry<<7)); carry=next; }
    }
}
static int8_t compare(const uint8_t *x,const uint8_t *y) {
    uint8_t i=6;
    while(i--) if(x[i]!=y[i]) return x[i]<y[i]?-1:1;
    return 0;
}
static void subtract(uint8_t *x,const uint8_t *y) {
    uint8_t i;
    uint16_t value,borrow=0;
    for(i=0;i<6;++i) {
        value=(uint16_t)y[i]+borrow;
        borrow=x[i]<value;
        x[i]=(uint8_t)(x[i]-value);
    }
}
static void unpack(const uint8_t *value,uint8_t *m,int16_t *e,uint8_t *negative) {
    clear(m,6); *e=value[0]; *negative=0;
    if(!*e) return;
    copy(m,value+1,5); *negative=m[4]>>7; m[4]|=128;
}
static void pack(uint8_t *m,int16_t exponent,uint8_t negative) {
    clear(zr_result,6);
    if(!nonzero(m)) return;
    while(m[5]) { right(m,1); ++exponent; }
    while(!(m[4]&128)) { left(m); --exponent; }
    if(exponent<1) return;
    if(exponent>255) { zr_error=1; return; }
    zr_result[0]=(uint8_t)exponent; copy(zr_result+1,m,5);
    zr_result[5]=(uint8_t)((zr_result[5]&127)|(negative?128:0));
}
static void add_values(void) {
    uint8_t i,sign;
    uint16_t carry=0,value;
    int16_t exponent=exponent_a>exponent_b?exponent_a:exponent_b;
    int8_t order;
    right(a,(uint16_t)(exponent-exponent_a));
    right(b,(uint16_t)(exponent-exponent_b));
    sign=negative_a;
    if(negative_a==negative_b) {
        for(i=0;i<6;++i) {
            value=(uint16_t)a[i]+b[i]+carry; a[i]=(uint8_t)value; carry=value>>8;
        }
    } else {
        order=compare(a,b);
        if(order>=0) subtract(a,b);
        else { subtract(b,a); copy(a,b,6); sign=negative_b; }
    }
    pack(a,exponent,sign);
}
static void multiply_values(void) {
    uint8_t i,j,shift;
    uint16_t carry,value;
    int16_t exponent;
    clear(work,10);
    for(i=0;i<5;++i) {
        carry=0;
        for(j=0;j<5;++j) {
            value=(uint16_t)a[i]*b[j]+work[i+j]+carry;
            work[i+j]=(uint8_t)value; carry=value>>8;
        }
        work[i+5]=(uint8_t)carry;
    }
    /* Normalized product has either 79 or 80 significant bits. */
    shift=(work[9]&128)?40:39;
    exponent=exponent_a+exponent_b-(shift==40?128:129);
    for(i=0;i<5;++i)
        a[i]=shift==40?work[i+5]:(uint8_t)((work[i+4]>>7)|(work[i+5]<<1));
    a[5]=0; pack(a,exponent,negative_a^negative_b);
}
static void divide_values(void) {
    uint8_t count;
    int16_t exponent;
    if(!exponent_b) { zr_error=2; return; }
    if(!exponent_a) { clear(zr_result,6); return; }
    clear(work,6);
    exponent=exponent_a-exponent_b+129;
    if(compare(a,b)>=0) { subtract(a,b); work[0]=1; count=39; }
    else { --exponent; count=40; }
    while(count--) {
        left(a); left(work);
        if(compare(a,b)>=0) { subtract(a,b); work[0]|=1; }
    }
    pack(work,exponent,negative_a^negative_b);
}
static void ordinal_result(uint8_t round) {
    uint16_t magnitude;
    uint8_t extra=0;
    int16_t shift=168-exponent_a;
    zr_integer=0;
    if(!exponent_a) return;
    if(exponent_a>144) { zr_error=0x92; return; }
    if(round&&exponent_a>=128) extra=(a[(shift-1)>>3]>>((shift-1)&7))&1;
    right(a,(uint16_t)shift);
    magnitude=(uint16_t)a[0]|((uint16_t)a[1]<<8);
    /* E<=144 guarantees magnitude<=65535. Check before incrementing. */
    if(magnitude>32768u) { zr_error=0x92; return; }
    magnitude+=extra;
    if(magnitude>(negative_a?32768u:32767u)) { zr_error=0x92; return; }
    zr_integer=negative_a?(int16_t)(0u-magnitude):(int16_t)magnitude;
}
static void integral_part(uint8_t fraction) {
    uint8_t i,mask;
    int16_t bits=168-exponent_a;
    if(bits>=40) { if(!fraction) clear(a,6); }
    else if(bits<=0) { if(fraction) clear(a,6); }
    else for(i=0;i<5;++i) {
        mask=bits>=8?255:bits<=0?0:(uint8_t)((1u<<bits)-1u);
        a[i]&=fraction?mask:(uint8_t)~mask; bits-=8;
    }
    pack(a,exponent_a,negative_a);
}
static void evaluate(uint8_t operation) {
    int8_t order;
    uint16_t magnitude;
    zr_error=0; clear(zr_result,6);
    unpack(zr_a,a,&exponent_a,&negative_a);
    unpack(zr_b,b,&exponent_b,&negative_b);
    switch(operation) {
    case ZR_ADD: add_values(); break;
    case ZR_SUB: if(exponent_b) negative_b^=1; add_values(); break;
    case ZR_SQR: copy(b,a,6); exponent_b=exponent_a; negative_b=negative_a;
        /* fall through */
    case ZR_MUL: multiply_values(); break;
    case ZR_DIV: divide_values(); break;
    case ZR_COMPARE:
        if(!exponent_a&&!exponent_b) order=0;
        else if(negative_a!=negative_b) order=negative_a?-1:1;
        else {
            order=exponent_a==exponent_b?compare(a,b):exponent_a<exponent_b?-1:1;
            if(negative_a) order=-order;
        }
        zr_integer=order; break;
    case ZR_FROM_INT:
        negative_a=zr_integer<0;
        magnitude=negative_a?(uint16_t)(0u-(uint16_t)zr_integer):(uint16_t)zr_integer;
        clear(a,6); a[0]=(uint8_t)magnitude; a[1]=(uint8_t)(magnitude>>8);
        pack(a,168,negative_a); break;
    case ZR_TRUNC: ordinal_result(0); break;
    case ZR_ROUND: ordinal_result(1); break;
    case ZR_NEGATE: pack(a,exponent_a,!negative_a); break;
    case ZR_ABS: pack(a,exponent_a,0); break;
    case ZR_INT: integral_part(0); break;
    case ZR_FRAC: integral_part(1); break;
    default: zr_error=0xff; break;
    }
}
static uint8_t digit(uint8_t c) { return c>='0'&&c<='9'; }
static void decimal_error(uint16_t position) {
    zr_error=57; zr_integer=(int16_t)(position+1); clear(zr_result,6);
}
static void times_ten(const uint8_t *value) {
    copy(zr_a,value,6); copy(zr_b,powers[0],6);
    zr_b[0]=132; zr_b[5]=32; /* 10 */
    evaluate(ZR_MUL);
}
static uint8_t decimal_scale_value(int16_t scale,uint16_t position) {
    uint16_t magnitude;
    uint8_t extra;
    if(scale< -38||scale>37) { decimal_error(position); return 0; }
    magnitude=(uint16_t)(scale<0?-scale:scale);
    copy(decimal_scale,powers[magnitude/4],6);
    extra=(uint8_t)(magnitude%4);
    while(extra--) {
        times_ten(decimal_scale);
        if(zr_error) { decimal_error(position); return 0; }
        copy(decimal_scale,zr_result,6);
    }
    copy(zr_a,decimal_value,6); copy(zr_b,decimal_scale,6);
    evaluate(scale<0?ZR_DIV:ZR_MUL);
    if(zr_error) { decimal_error(position); return 0; }
    copy(decimal_value,zr_result,6); return 1;
}
static void decimal(void) {
    uint16_t p=0,count=0,exponent=0;
    int16_t scale=0;
    uint8_t negative=0,negative_exponent=0,c;
    clear(decimal_value,6);
    if(p<zr_length&&(zr_text[p]=='+'||zr_text[p]=='-')) negative=zr_text[p++]=='-';
    if(p==zr_length||!digit(zr_text[p])) { decimal_error(p); return; }
    while(p<zr_length) {
        c=zr_text[p];
        if(c=='.'&&!count) { count=1; ++p; continue; }
        if(!digit(c)) break;
        times_ten(decimal_value);
        if(zr_error) { decimal_error(p); return; }
        copy(decimal_value,zr_result,6);
        zr_integer=c-'0'; evaluate(ZR_FROM_INT); copy(zr_b,zr_result,6);
        copy(zr_a,decimal_value,6); evaluate(ZR_ADD);
        if(zr_error) { decimal_error(p); return; }
        copy(decimal_value,zr_result,6);
        if(count) --scale;
        ++p;
    }
    /* The fractional scale is applied before reading a separate exponent.
     * Keeping these two truncated operations distinct is observable. */
    if(!decimal_scale_value(scale,p)) return;
    if(p<zr_length&&(zr_text[p]=='E'||zr_text[p]=='e')) {
        ++p;
        if(p<zr_length&&(zr_text[p]=='+'||zr_text[p]=='-')) negative_exponent=zr_text[p++]=='-';
        if(p==zr_length||!digit(zr_text[p])) { decimal_error(p); return; }
        exponent=zr_text[p++]-'0';
        if(p<zr_length&&digit(zr_text[p])) exponent=exponent*10+zr_text[p++]-'0';
        if(!decimal_scale_value(negative_exponent?-(int16_t)exponent:(int16_t)exponent,p)) return;
    }
    if(p!=zr_length) { decimal_error(p); return; }
    copy(zr_result,decimal_value,6);
    if(negative&&zr_result[0]) zr_result[5]^=128;
    zr_integer=0;
}
void zr_eval(void) REAL_API {
    if(zr_op==ZR_DECIMAL) decimal(); else evaluate(zr_op);
}
