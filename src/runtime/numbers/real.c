#include "runtime/numbers/real.h"

/* Real6 is sign * M * 2^(E-168), with a normalized 40-bit M.
 * Six-byte working magnitudes keep the extra carry needed by add/divide.
 * All discarded arithmetic bits are truncated, never IEEE rounded. */
uint8_t zr_op,zr_error,zr_a[6],zr_b[6],zr_result[6];
int16_t zr_integer;
const uint8_t *zr_text;
uint16_t zr_length;
#ifndef ZP_REAL_COMPILER
uint16_t zr_code;
uint8_t zr_assign;
int16_t zr_width,zr_places;
uint8_t zr_formatted[66];
#endif
/* Test-only compact profile: keep the exact integer Val parser below while
 * excluding the Real engine. Normal/compiler builds retain their behavior. */
#ifndef ZP_INTEGER_RUNTIME
static uint8_t a[6],b[6],work[10],negative_a,negative_b;
static int16_t exponent_a,exponent_b;
static uint8_t decimal_value[6],decimal_scale[6];
#ifndef ZP_REAL_COMPILER
static uint8_t format_digits[12];
static int16_t format_exponent;
#endif
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
#ifndef ZP_REAL_COMPILER
/* The exact radicand is M<<39 for odd E and M<<40 for even E.
 * Forty restoring base-four steps produce floor(sqrt(radicand)); packing
 * that integer gives an error below one final Real6 ULP for every input.
 * a is the remainder, b the partial root, work the immutable80-bit input. */
static void square_root(void) {
    uint8_t i,take;
    int16_t exponent;
    if(!exponent_a) return;
    if(negative_a) {zr_error=3;return;}
    exponent=(exponent_a+129)/2;
    clear(work,10);
    for(i=0;i<5;++i) {
        if(exponent_a&1) {
            work[i+4]|=(uint8_t)(a[i]<<7);
            work[i+5]=a[i]>>1;
        } else work[i+5]=a[i];
    }
    clear(a,6);clear(b,6);
    i=40;
    while(i--) {
        left(a);left(a);
        a[0]|=(work[i>>2]>>((i&3)*2))&3;
        left(b);left(b);b[0]|=1;
        take=compare(a,b)>=0;
        if(take) subtract(a,b);
        right(b,1);b[0]|=take;
    }
    pack(b,exponent,0);
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
#endif
static void evaluate(uint8_t operation) {
#ifndef ZP_REAL_COMPILER
    int8_t order;
#endif
    uint16_t magnitude;
    zr_error=0; clear(zr_result,6);
    unpack(zr_a,a,&exponent_a,&negative_a);
    unpack(zr_b,b,&exponent_b,&negative_b);
    switch(operation) {
    case ZR_ADD: add_values(); break;
#ifndef ZP_REAL_COMPILER
    case ZR_SUB: if(exponent_b) negative_b^=1; add_values(); break;
    case ZR_SQR: copy(b,a,6); exponent_b=exponent_a; negative_b=negative_a;
        /* fall through */
#endif
    case ZR_MUL: multiply_values(); break;
    case ZR_DIV: divide_values(); break;
#ifndef ZP_REAL_COMPILER
    case ZR_COMPARE:
        if(!exponent_a&&!exponent_b) order=0;
        else if(negative_a!=negative_b) order=negative_a?-1:1;
        else {
            order=exponent_a==exponent_b?compare(a,b):exponent_a<exponent_b?-1:1;
            if(negative_a) order=-order;
        }
        zr_integer=order; break;
#endif
    case ZR_FROM_INT:
        negative_a=zr_integer<0;
        magnitude=negative_a?(uint16_t)(0u-(uint16_t)zr_integer):(uint16_t)zr_integer;
        clear(a,6); a[0]=(uint8_t)magnitude; a[1]=(uint8_t)(magnitude>>8);
        pack(a,168,negative_a); break;
#ifndef ZP_REAL_COMPILER
    case ZR_TRUNC: ordinal_result(0); break;
    case ZR_ROUND: ordinal_result(1); break;
    case ZR_NEGATE: pack(a,exponent_a,!negative_a); break;
    case ZR_ABS: pack(a,exponent_a,0); break;
    case ZR_INT: integral_part(0); break;
    case ZR_FRAC: integral_part(1); break;
    case ZR_SQRT: square_root(); break;
#endif
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
static void build_scale(uint8_t magnitude) {
    uint8_t extra;
    copy(decimal_scale,powers[magnitude/4],6);
    extra=(uint8_t)(magnitude%4);
    while(extra--) {
        times_ten(decimal_scale);
        copy(decimal_scale,zr_result,6);
    }
}
static uint8_t decimal_scale_value(int16_t scale,uint16_t position) {
    if(scale< -38||scale>37) { decimal_error(position); return 0; }
    build_scale((uint8_t)(scale<0?-scale:scale));
    if(zr_error) { decimal_error(position); return 0; }
    copy(zr_a,decimal_value,6); copy(zr_b,decimal_scale,6);
    evaluate(scale<0?ZR_DIV:ZR_MUL);
    if(zr_error) { decimal_error(position); return 0; }
    copy(decimal_value,zr_result,6); return 1;
}
static void decimal(uint8_t runtime) {
    uint16_t p=0,count=0,exponent=0;
    int16_t scale=0;
    uint8_t negative=0,negative_exponent=0,c;
    clear(decimal_value,6);
    if(p<zr_length&&(zr_text[p]=='-'||(!runtime&&zr_text[p]=='+')))
        negative=zr_text[p++]=='-';
    if(runtime) {
        if(p<zr_length&&!digit(zr_text[p])&&zr_text[p]!='.'&&zr_text[p]!='e'&&zr_text[p]!='E')
            { decimal_error(p); return; }
    } else if(p==zr_length||!digit(zr_text[p])) { decimal_error(p); return; }
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
#else
static uint8_t digit(uint8_t c) {return c>='0'&&c<='9';}
#endif /* !ZP_INTEGER_RUNTIME */
#ifndef ZP_REAL_COMPILER
static void val_integer(void) {
    uint16_t p=0,magnitude=0;
    uint8_t negative=0,hexadecimal=0,c,d;
    if(zr_text[p]=='-') {negative=1;++p;}
    if(p<zr_length&&zr_text[p]=='$') {hexadecimal=1;++p;}
    for(;p<zr_length;++p) {
        c=zr_text[p];
        d=digit(c)?c-'0':c>='A'&&c<='F'?c-'A'+10:
            c>='a'&&c<='f'?c-'a'+10:255;
        if((hexadecimal&&(d>=16||magnitude>4095))||
           (!hexadecimal&&(d>=10||magnitude>6553||(magnitude==6553&&d>5)))) {
            zr_code=p+1;return;
        }
        magnitude=magnitude*(hexadecimal?16:10)+d;
    }
    if(!hexadecimal&&magnitude>(negative?32768u:32767u)) {zr_code=p+1;return;}
    zr_integer=negative?(int16_t)(0u-magnitude):(int16_t)magnitude;
    zr_assign=1;
}
#ifndef ZP_INTEGER_RUNTIME
static void val(void) {
    zr_error=0;zr_code=0;zr_assign=0;zr_integer=0;clear(zr_result,6);
    if(!zr_length) return;
    if(zr_op==ZR_VAL_INTEGER) {val_integer();return;}
    decimal(1);
    if(zr_error) {zr_code=(uint16_t)zr_integer;zr_error=0;return;}
    zr_assign=1;
}
/* Normalize through Real6 arithmetic before extracting decimal digits. The
 * resulting scale/rounding differs observably from formatting the raw exact
 * binary rational. Magnitudes now lie in[1,10), so six bytes hold every
 * fractional times-ten product without losing bits. */
static void formatted_digits(void) {
    uint8_t i,j,shift,at,bits;
    uint16_t v,carry;
    int16_t estimate;
    clear(format_digits,12);format_exponent=0;
    if(!decimal_value[0]) return;
    estimate=((int16_t)decimal_value[0]-128)*77+5;
    format_exponent=estimate>=0?estimate/256:-((255-estimate)/256);
    if(format_exponent< -38) format_exponent=-38;
    build_scale((uint8_t)(format_exponent<0?-format_exponent:format_exponent));
    copy(zr_a,decimal_value,6);zr_a[5]&=127;
    copy(zr_b,decimal_scale,6);
    evaluate(format_exponent<0?ZR_MUL:ZR_DIV);
    if(zr_result[0]<129) {
        copy(decimal_value,zr_result,6);times_ten(decimal_value);
        --format_exponent;
    }
    clear(a,6);copy(a,zr_result+1,5);a[4]|=128;
    shift=168-zr_result[0];at=shift>>3;bits=shift&7;
    for(i=0;i<12;++i) {
        format_digits[i]=(uint8_t)((a[at]>>bits)|((uint16_t)a[at+1]<<(8-bits)));
        a[at]&=(uint8_t)((1u<<bits)-1u);
        for(j=at+1;j<6;++j) a[j]=0;
        carry=0;
        for(j=0;j<6;++j) {v=(uint16_t)a[j]*10+carry;a[j]=(uint8_t)v;carry=v>>8;}
    }
}
static void round_formatted(int16_t keep) {
    int16_t i;
    if(keep>11) keep=11;
    if(keep<=0) {
        i=keep==0&&format_digits[0]>=5;
        clear(format_digits,12);
        if(i) {format_digits[0]=1;++format_exponent;}else format_exponent=0;
        return;
    }
    if(format_digits[keep]>=5) {
        i=keep;
        while(i&&format_digits[i-1]==9) format_digits[--i]=0;
        if(i) ++format_digits[i-1];else {format_digits[0]=1;++format_exponent;}
    }
    for(i=keep;i<12;++i) format_digits[i]=0;
}
static void append_formatted(uint8_t c) {zr_formatted[++zr_formatted[0]]=c;}
static uint8_t formatted_digit(int16_t at) {return at<0||at>=11?0:format_digits[at];}
static void format_value(void) {
    uint8_t negative,sign_column,n,i;
    int16_t position;
    copy(decimal_value,zr_a,6);zr_error=0;zr_formatted[0]=0;
    sign_column=(decimal_value[5]&128)!=0;
    negative=decimal_value[0]&&sign_column;
    formatted_digits();
    if(zr_places>=0&&zr_places<=24) {
        round_formatted(format_exponent+zr_places+1);
        if(negative) append_formatted('-');
        if(format_exponent<0) append_formatted('0');
        else for(position=0;position<=format_exponent;++position)
            append_formatted('0'+formatted_digit(position));
        if(zr_places) append_formatted('.');
        for(position=1;position<=zr_places;++position)
            append_formatted('0'+formatted_digit(format_exponent+position));
    } else {
        if(zr_width<=7+sign_column) n=1;
        else if(zr_width>=16+sign_column) n=10;
        else n=(uint8_t)(zr_width-6-sign_column);
        round_formatted(n+1);
        if(negative) append_formatted('-');
        append_formatted('0'+format_digits[0]);append_formatted('.');
        for(i=1;i<=n;++i) append_formatted('0'+format_digits[i]);
        append_formatted('E');append_formatted(format_exponent<0?'-':'+');
        position=format_exponent<0?-format_exponent:format_exponent;
        append_formatted('0'+position/10);append_formatted('0'+position%10);
    }
}
#endif /* !ZP_INTEGER_RUNTIME */
#endif /* !ZP_REAL_COMPILER */
void zr_eval(void) REAL_API {
#ifdef ZP_INTEGER_RUNTIME
    uint8_t i;
    zr_error=0;zr_code=0;zr_assign=0;zr_integer=0;
    for(i=0;i<6;++i)zr_result[i]=0;
    if(zr_op!=ZR_VAL_INTEGER) {zr_error=0xff;return;}
    if(zr_length)val_integer();
#else
    if(zr_op==ZR_DECIMAL) decimal(0);
#ifdef ZP_REAL_COMPILER
    /* Only literals and typed-constant integer conversion execute inside the
     * compiler. Pascal arithmetic is emitted as calls into the full runtime. */
    else if(zr_op==ZR_FROM_INT) evaluate(ZR_FROM_INT);
    else { zr_error=0xff;clear(zr_result,6); }
#else
    else if(zr_op==ZR_FORMAT) format_value();
    else if(zr_op==ZR_VAL_INTEGER||zr_op==ZR_VAL_REAL) val();
    else if(zr_op>=ZR_LN&&zr_op<=ZR_ARCTAN) zr_transcendental();
    else evaluate(zr_op);
#endif
#endif /* ZP_INTEGER_RUNTIME */
}
