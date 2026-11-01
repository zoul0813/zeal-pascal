#include "compiler_late.h"

/* Private helpers belong to this image's relocatable code/static payload.
 * They share the ordinary relocation ledger and never grow the fixed runtime
 * prefix or acquire an OS resource. All entries preserve IX/IY and alternates. */
static void engine(void) {
    uint16_t skip;
    if(cc.error->code||cc.suppress) return;
    if(!cc.random_entry&&!cc_root_member()) {
        /* The initial jump skips both writable state and callable code. A
         * later call, even from another routine, reuses this one definition. */
        skip=cc_forward(0xc3);
        cc.random_state=cc.emitted;
        cc_emit(0); cc_emit(0); cc_emit(0); cc_emit(0);
        cc.random_entry=cc.emitted;
        cc_address(0x2a,cc.random_state);
        cc_emit(0xed); cc_address(0x5b,cc.random_state+2);
        /* Pinned CP/M-80 TP3: state = 129*state + $361962E9, modulo2^32.
         * Derivation and original vectors are bound in tp3-fidelity.json. */
        cc_emit(0xd5); cc_emit(0xe5);
        cc_emit(0x06); cc_emit(7);
        cc_emit(0x29); cc_emit(0xcb); cc_emit(0x13); cc_emit(0xcb); cc_emit(0x12);
        cc_emit(0x10); cc_emit(249); /* shift left7 */
        cc_emit(0xc1); cc_emit(0x09); /* original low word and carry */
        cc_emit(0xc1); cc_emit(0xeb); cc_emit(0xed); cc_emit(0x4a); cc_emit(0xeb);
        cc_op16(0x01,0x62e9); cc_emit(0x09);
        cc_emit(0xeb); cc_op16(0x01,0x3619); cc_emit(0xed); cc_emit(0x4a); cc_emit(0xeb);
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
    uint16_t loop,ready,nonzero,done;
    /* Saved generator state and the bound remain under the native guard. */
    cc_reserve(16);
    if(bounded) {
        cc_op16(0x11,1); cc_op16(0x01,32767); cc_call(RT_RANGE);
        cc_emit(0xe5); /* evaluate and retain the bound once */
        draw();
        cc_emit(0x62); cc_emit(0x6b); /* high word of updated state */
        cc_emit(0xcb); cc_emit(0x3c); cc_emit(0xcb); cc_emit(0x1d);
        cc_emit(0xd1); cc_emit(0xeb); cc_call(RT_MOD);
    } else {
        draw();
        cc_emit(0x7c); cc_emit(0xb5); cc_emit(0xb2); cc_emit(0xb3);
        nonzero=cc_forward(0xc2);
        cc_op16(0x21,RT_REAL_BUFFER); cc_op16(0x01,6); cc_emit(0xaf); cc_call(RT_FILL);
        done=cc_forward(0xc3); cc_patch(nonzero);
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
        cc_patch(done); cc_op16(0x21,RT_REAL_BUFFER); cc_real_hold();
    }
}
void cc_randomize(void) FRONTEND {
    if(cc.error->code||cc.suppress) return;
    cc_reserve(16); engine();
    /* TP3 changes only the most significant seed byte using Z80 R. */
    cc_emit(0xed); cc_emit(0x5f);
    cc_root_address(0x32,LATE_ROOT,LATE_RANDOM_STATE,cc.random_state,3);
}
