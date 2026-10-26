#include "zp.h"
#include "real.h"
#ifndef __SDCC
#include <stdio.h>
#endif

/* This tests the shared arithmetic component directly on Zeal. Pascal syntax,
 * generated-code entry points and full Real6 I/O have separate acceptance. */
typedef struct {
    uint8_t op,a[6],b[6],out[6],error;
    int16_t input,value;
} RealVector;
typedef struct { const char *text; uint8_t out[6],error; } DecimalVector;
typedef struct { uint8_t value[6];int16_t width,places;const char *text; } FormatVector;
typedef struct {
    uint8_t op;const char *text;uint16_t length,code;uint8_t assigned;
    int16_t value;uint8_t out[6];
} ValVector;
typedef struct {uint8_t value[5],odd[5],even[5];} SqrtScaleVector;
#include "real_vectors.h"
volatile uint16_t test_passed,test_failed;
volatile uint8_t test_done;
uint8_t test_stop[3]={0x76,0x18,0xfd};
#ifdef __SDCC
static uint8_t real_before[7],real_after[7],real_guard_ok;
extern void test_machine(uint8_t *state) NATIVE;
extern void test_complete(void);
#endif
static void check(uint8_t ok) { if(ok) ++test_passed; else ++test_failed; }
#ifdef __SDCC
/* Same observation call depth on both sides of the actual C engine. The
 * canary lies beyond a conservative 256-byte helper reservation. */
static void checked_eval(void) __naked {
    __asm
    ld hl,#-272
    add hl,sp
    ld b,#16
001$:
    ld (hl),#0xa5
    inc hl
    djnz 001$
    ld hl,#_real_before
    call _test_machine
    call _zr_eval
    ld hl,#_real_after
    call _test_machine
    ld hl,#-272
    add hl,sp
    ld b,#16
002$:
    ld a,(hl)
    cp #0xa5
    jr nz,003$
    inc hl
    djnz 002$
    ld a,#1
    jr 004$
003$:
    xor a
004$:
    ld (_real_guard_ok),a
    ret
    __endasm;
}
static void run_engine(void) {
    checked_eval(); check(!memcmp(real_before,real_after,7)); check(real_guard_ok);
}
#else
static void run_engine(void) { zr_eval(); }
#endif
int main(void) {
    uint16_t i,exponent;
    const RealVector *v;
    const DecimalVector *d;
    for(i=0;i<sizeof(real_vectors)/sizeof(real_vectors[0]);++i) {
        v=real_vectors+i; zr_op=v->op; zr_integer=v->input;
        memcpy(zr_a,v->a,6); memcpy(zr_b,v->b,6);
#ifdef __SDCC
        if(i&1) { __asm di __endasm; }
#endif
        run_engine();
#ifdef __SDCC
        if(i&1) { __asm ei __endasm; }
#endif
        check(zr_error==v->error);
        if(!v->error) {
            check(!memcmp(zr_result,v->out,6));
            if(v->op==ZR_COMPARE||v->op==ZR_TRUNC||v->op==ZR_ROUND) check(zr_integer==v->value);
        }
    }
    for(i=0;i<sizeof(decimal_vectors)/sizeof(decimal_vectors[0]);++i) {
        d=decimal_vectors+i; zr_op=ZR_DECIMAL;
        zr_text=(const uint8_t*)d->text; zr_length=(uint16_t)strlen(d->text);
        run_engine();
        check(zr_error==d->error);
        if(!d->error) { check(!memcmp(zr_result,d->out,6)); check(zr_integer==0); }
    }
    for(exponent=1;exponent<=255;++exponent) {
        for(i=0;i<sizeof(sqrt_scales)/sizeof(sqrt_scales[0]);++i) {
            const SqrtScaleVector *v=sqrt_scales+i;
            zr_op=ZR_SQRT;zr_a[0]=(uint8_t)exponent;memcpy(zr_a+1,v->value,5);
#ifdef __SDCC
            if(exponent&1) { __asm di __endasm; }
#endif
            run_engine();
#ifdef __SDCC
            if(exponent&1) { __asm ei __endasm; }
#endif
            check(!zr_error&&zr_result[0]==(exponent+129)/2);
            check(!memcmp(zr_result+1,exponent&1?v->odd:v->even,5));
            zr_a[5]|=128;run_engine();check(zr_error==3);
        }
    }
    for(i=0;i<sizeof(format_vectors)/sizeof(format_vectors[0]);++i) {
        const FormatVector *f=format_vectors+i;
        memcpy(zr_a,f->value,6);zr_width=f->width;zr_places=f->places;zr_op=ZR_FORMAT;
#ifdef __SDCC
        if(i&1) { __asm di __endasm; }
#endif
        run_engine();
#ifdef __SDCC
        if(i&1) { __asm ei __endasm; }
#endif
        check(!zr_error&&zr_formatted[0]<=65);
        check(zr_formatted[0]==strlen(f->text));
        check(!memcmp(zr_formatted+1,f->text,strlen(f->text)));
    }
    for(i=0;i<sizeof(val_vectors)/sizeof(val_vectors[0]);++i) {
        const ValVector *v=val_vectors+i;
        zr_op=v->op;zr_text=(const uint8_t*)v->text;zr_length=v->length;
        zr_code=32767;zr_assign=255;zr_error=255;
#ifdef __SDCC
        if(i&1) { __asm di __endasm; }
#endif
        run_engine();
#ifdef __SDCC
        if(i&1) { __asm ei __endasm; }
#endif
        check(!zr_error&&zr_code==v->code&&zr_assign==v->assigned);
        if(v->assigned) check(v->op==ZR_VAL_INTEGER?zr_integer==v->value:!memcmp(zr_result,v->out,6));
    }
#ifdef __SDCC
    test_done=255; test_complete(); return 0;
#else
    printf("Real6 assertions: %u passed, %u failed\n",test_passed,test_failed);
    return test_failed?1:0;
#endif
}
