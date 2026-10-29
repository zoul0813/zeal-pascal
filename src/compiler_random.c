#include "compiler_late.h"

/* Private helpers belong to this image's relocatable code/static payload.
 * They share the ordinary relocation ledger and never grow the fixed runtime
 * prefix or acquire an OS resource. All entries preserve IX/IY and alternates. */
static void xor_saved(void) {
    cc_emit(0xc1); /* BC = original low word */
    cc_emit(0x7d); cc_emit(0xa9); cc_emit(0x6f);
    cc_emit(0x7c); cc_emit(0xa8); cc_emit(0x67);
    cc_emit(0xc1); /* BC = original high word */
    cc_emit(0x7b); cc_emit(0xa9); cc_emit(0x5f);
    cc_emit(0x7a); cc_emit(0xa8); cc_emit(0x57);
}
static void shift_xor(uint8_t count,uint8_t right) {
    uint16_t loop;
    cc_emit(0xd5); cc_emit(0xe5); /* retain the full original DEHL */
    cc_emit(0x06); cc_emit(count);
    loop=cc.emitted;
    if(right) {
        cc_emit(0xcb); cc_emit(0x3a); /* SRL D */
        cc_emit(0xcb); cc_emit(0x1b); /* RR E */
        cc_emit(0xcb); cc_emit(0x1c); /* RR H */
        cc_emit(0xcb); cc_emit(0x1d); /* RR L */
    } else {
        cc_emit(0x29);              /* ADD HL,HL */
        cc_emit(0xcb); cc_emit(0x13); /* RL E */
        cc_emit(0xcb); cc_emit(0x12); /* RL D */
    }
    cc_emit(0x10); cc_emit((uint8_t)(loop-cc.emitted-1));
    xor_saved();
}
static void engine(void) {
    uint16_t skip;
    if(cc.error->code||cc.suppress) return;
    if(!cc.random_entry&&!cc_root_member()) {
        /* The initial jump skips both writable state and callable code. A
         * later call, even from another routine, reuses this one definition. */
        skip=cc_forward(0xc3);
        cc.random_state=cc.emitted;
        cc_emit(0xf5); cc_emit(0x79); cc_emit(0x2b); cc_emit(0x6d);
        cc.random_entry=cc.emitted;
        cc_address(0x2a,cc.random_state);
        cc_emit(0xed); cc_address(0x5b,cc.random_state+2);
        /* Marsaglia xorshift32 (13,17,5), period 2^32-1 from any nonzero state. */
        shift_xor(13,0); shift_xor(17,1); shift_xor(5,0);
        cc_address(0x22,cc.random_state);
        cc_emit(0xed); cc_address(0x53,cc.random_state+2);
        cc_emit(0xc9);
        cc_patch(skip);
        cc_root_bind(LATE_ROOT,LATE_RANDOM,cc.random_entry);cc_root_bind(LATE_ROOT,LATE_RANDOM_STATE,cc.random_state);
    }
}
static void draw(void) {engine();cc_root_address(0xcd,LATE_ROOT,LATE_RANDOM,cc.random_entry,0);}
void cc_random_finish(void) FRONTEND {
    if(cc_root_pending(LATE_ROOT,LATE_RANDOM)||cc_root_pending(LATE_ROOT,LATE_RANDOM_STATE))engine();
}
void cc_random(uint8_t bounded) FRONTEND {
    uint16_t loop,retry,ready;
    /* Four saved state bytes and a return word, plus the bounded draw's two
     * retained words. Existing guard also retains its emergency headroom. */
    cc_reserve(16);
    if(bounded) {
        cc_op16(0x11,1); cc_op16(0x01,32767); cc_call(RT_RANGE);
        cc_emit(0xe5); /* bound, evaluated once */
        cc_op16(0x11,0x8000u); cc_call(RT_MOD);
        /* MOD(-32768,N) is negative or zero: threshold = 32768 mod N. */
        cc_emit(0xaf); cc_emit(0x95); cc_emit(0x6f);
        cc_emit(0x9f); cc_emit(0x94); cc_emit(0x67);
        cc_emit(0xe5); /* threshold */
        loop=cc.emitted;
        draw();
        cc_emit(0xcb); cc_emit(0xbc); /* RES 7,H: 0..32767 candidate */
        cc_emit(0xc1); cc_emit(0xc5); /* BC = threshold, retained */
        cc_emit(0xb7); cc_emit(0xed); cc_emit(0x42); /* candidate-threshold */
        retry=cc_forward(0xda); /* rejected candidates advance the same state */
        if(!cc.error->code&&!cc.suppress) cc_word(retry,loop);
        cc_emit(0x09); /* restore accepted candidate */
        cc_emit(0xc1); cc_emit(0xd1); cc_emit(0xeb);
        cc_call(RT_MOD); /* positive candidate MOD positive bound */
    } else {
        draw();
        cc_emit(0x3e); cc_emit(128);
        loop=cc.emitted;
        cc_emit(0xcb); cc_emit(0x7a); /* normalize DEHL to a Real6 mantissa */
        ready=cc_forward(0xc2);
        cc_emit(0x29); cc_emit(0xcb); cc_emit(0x13); cc_emit(0xcb); cc_emit(0x12);
        cc_emit(0x3d); cc_address(0xc3,loop);
        cc_patch(ready);
        cc_op16(0x32,RT_REAL_BUFFER);
        cc_op16(0x22,RT_REAL_BUFFER+2);
        cc_emit(0x7b); cc_op16(0x32,RT_REAL_BUFFER+4);
        cc_emit(0x7a); cc_emit(0xe6); cc_emit(127); cc_op16(0x32,RT_REAL_BUFFER+5);
        cc_emit(0xaf); cc_op16(0x32,RT_REAL_BUFFER+1);
        cc_op16(0x21,RT_REAL_BUFFER); cc_real_hold();
    }
}
void cc_randomize(void) FRONTEND {
    uint16_t fallback,seeded,nonzero;
    cc_reserve(16);
    draw(); cc_emit(0xd5); cc_emit(0xe5);
    cc_op16(0x21,20); cc_emit(0xcf); /* H=0 clock, L=gettime syscall */
    cc_emit(0xb7); fallback=cc_forward(0xc2);
    seeded=cc_forward(0xc3);
    cc_patch(fallback);
    cc_emit(0xed); cc_emit(0x5f); cc_emit(0x57); /* LD A,R; D=A */
    cc_emit(0xed); cc_emit(0x5f); cc_emit(0x5f); /* LD A,R; E=A */
    cc_patch(seeded);
    cc_emit(0xc1); /* BC = previous low state; mix the varying timer seed */
    cc_emit(0x7b); cc_emit(0xa9); cc_emit(0x6f);
    cc_emit(0x7a); cc_emit(0xa8); cc_emit(0x67);
    cc_emit(0xd1); /* previous high state */
    cc_emit(0x7c); cc_emit(0xb5); cc_emit(0xb2); cc_emit(0xb3);
    nonzero=cc_forward(0xc2);
    cc_emit(0x23); /* xorshift's one excluded state must never be installed */
    cc_patch(nonzero);
    cc_root_address(0x22,LATE_ROOT,LATE_RANDOM_STATE,cc.random_state,0);
    cc_emit(0xed); cc_root_address(0x53,LATE_ROOT,LATE_RANDOM_STATE,cc.random_state,2);
}
