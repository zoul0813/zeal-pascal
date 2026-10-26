#include "real.h"
#include <string.h>
/* Private Q96 intermediates; public Real6 primitives retain their own rules.
 * All state is invocation-owned. No syscall or callback occurs here. */
#define N 13
#define FRACTION_BITS 96
typedef uint8_t Fixed[N];
typedef struct {Fixed x,y,z,term,part,sum;uint8_t product[2*N];} Wide;
static const Fixed log_two={152,179,227,201,171,121,207,209,247,23,114,177,0};
static void zero(uint8_t *p,uint8_t n) {while(n--) *p++=0;}
static void one(Fixed q) {zero(q,N);q[N-1]=1;}
static int8_t order(const Fixed x,const Fixed y) {
    uint8_t i=N;while(i--) if(x[i]!=y[i]) return x[i]<y[i]?-1:1;return 0;
}
static void add(Fixed out,const Fixed x,const Fixed y) {
    uint8_t i;uint16_t v,carry=0;
    for(i=0;i<N;++i) {v=(uint16_t)x[i]+y[i]+carry;out[i]=(uint8_t)v;carry=v>>8;}
}
static void subtract(Fixed out,const Fixed x,const Fixed y) {
    uint8_t i;uint16_t v,borrow=0;
    for(i=0;i<N;++i) {v=(uint16_t)y[i]+borrow;borrow=x[i]<v;out[i]=(uint8_t)(x[i]-v);}
}
static void multiply(Fixed out,const Fixed x,const Fixed y,uint8_t *product) {
    uint8_t i,j;uint16_t v,carry;
    zero(product,2*N);
    for(i=0;i<N;++i) {
        carry=0;
        for(j=0;j<N;++j) {
            v=(uint16_t)x[i]*y[j]+product[i+j]+carry;
            product[i+j]=(uint8_t)v;carry=v>>8;
        }
        product[i+N]=(uint8_t)carry;
    }
    memcpy(out,product+FRACTION_BITS/8,N);
}
static void small_multiply(Fixed out,const Fixed x,uint8_t n) {
    uint8_t i;uint16_t v,carry=0;
    for(i=0;i<N;++i) {v=(uint16_t)x[i]*n+carry;out[i]=(uint8_t)v;carry=v>>8;}
}
static void small_divide(Fixed out,const Fixed x,uint8_t n) {
    uint8_t i=N;uint16_t v,remainder=0;
    while(i--) {v=(remainder<<8)|x[i];out[i]=(uint8_t)(v/n);remainder=v-(uint16_t)out[i]*n;}
}
/* The quotient is in[0,1]. Copy before clearing permits an aliased numerator. */
static void ratio(Fixed out,const Fixed x,const Fixed y,Fixed remainder) {
    uint8_t bit,i,carry,next;
    memcpy(remainder,x,N);zero(out,N);
    if(order(remainder,y)==0) {one(out);return;}
    bit=FRACTION_BITS;
    while(bit--) {
        carry=0;
        for(i=0;i<N;++i) {next=remainder[i]>>7;remainder[i]=(uint8_t)((remainder[i]<<1)|carry);carry=next;}
        if(order(remainder,y)>=0) {subtract(remainder,remainder,y);out[bit>>3]|=(uint8_t)(1u<<(bit&7));}
    }
}
static void load(Fixed out,uint8_t exponent) {
    uint8_t bit;int16_t at;
    zero(out,N);
    for(bit=0;bit<40;++bit) {
        at=(int16_t)bit+exponent-72;
        if((bit==39||(zr_a[1+(bit>>3)]&(1u<<(bit&7))))&&at>=0&&at<8*N)
            out[at>>3]|=(uint8_t)(1u<<(at&7));
    }
}
static void store(const Fixed value,uint8_t negative,int16_t scale) {
    int16_t high=8*N-1,at,exponent;uint8_t bit;
    while(high>=0&&!(value[high>>3]&(1u<<(high&7)))) --high;
    if(high<0) return;
    exponent=high-FRACTION_BITS+scale+129;
    if(exponent<1) return;
    if(exponent>255) {zr_error=1;return;}
    zr_result[0]=(uint8_t)exponent;
    for(bit=0;bit<39;++bit) {
        at=high-39+bit;
        if(at>=0&&(value[at>>3]&(1u<<(at&7)))) zr_result[1+(bit>>3)]|=(uint8_t)(1u<<(bit&7));
    }
    if(negative) zr_result[5]|=128;
}
static void logarithm(Wide *w) {
    int16_t scale;uint8_t negative,n;
    if(!zr_a[0]||(zr_a[5]&128)) {zr_error=4;return;}
    scale=(int16_t)zr_a[0]-129;
    if((zr_a[5]&127)>=64) {load(w->x,128);++scale;}else load(w->x,129);
    one(w->y);negative=order(w->x,w->y)<0;
    if(negative) subtract(w->z,w->y,w->x);else subtract(w->z,w->x,w->y);
    add(w->y,w->y,w->x);
    ratio(w->z,w->z,w->y,w->term);
    multiply(w->y,w->z,w->z,w->product);
    memcpy(w->term,w->z,N);memcpy(w->sum,w->z,N);
    for(n=3;n<=63;n+=2) {
        multiply(w->term,w->term,w->y,w->product);
        small_divide(w->part,w->term,n);add(w->sum,w->sum,w->part);
    }
    add(w->sum,w->sum,w->sum);
    if(!scale) {store(w->sum,negative,0);return;}
    small_multiply(w->x,log_two,(uint8_t)(scale<0?-scale:scale));
    if((scale<0)==negative) add(w->x,w->x,w->sum);else subtract(w->x,w->x,w->sum);
    store(w->x,scale<0,0);
}
static void exponential(Wide *w) {
    uint8_t negative=zr_a[5]>>7,n;int16_t scale=0;
    if(zr_a[0]<88) {zr_result[0]=129;return;}
    if(zr_a[0]>135) {if(!negative) zr_error=1;return;}
    load(w->x,zr_a[0]);zero(w->y,N);w->y[N-1]=89;
    if(order(w->x,w->y)>=0) {if(!negative) zr_error=1;return;}
    while(order(w->x,log_two)>=0) {subtract(w->x,w->x,log_two);++scale;}
    one(w->term);one(w->sum);
    for(n=1;n<=40;++n) {
        multiply(w->term,w->term,w->x,w->product);
        small_divide(w->term,w->term,n);add(w->sum,w->sum,w->term);
    }
    if(negative) {one(w->z);ratio(w->x,w->z,w->sum,w->term);store(w->x,0,-scale);}
    else store(w->sum,0,scale);
}
void zr_transcendental(void) REAL_API {
    Wide state;
    zero(zr_result,6);zr_error=0;
    if(zr_op==ZR_LN) logarithm(&state);
    else if(zr_op==ZR_EXP) exponential(&state);
    else zr_error=255;
}
