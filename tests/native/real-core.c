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
    uint16_t i;
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
#ifdef __SDCC
    test_done=255; test_complete(); return 0;
#else
    printf("Real6 assertions: %u passed, %u failed\n",test_passed,test_failed);
    return test_failed?1:0;
#endif
}
