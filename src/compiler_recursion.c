#include "compiler.h"

/* TP3 A- saves/restores the Pascal workspace on a separate recursion stack.
 * Native lexical/callback metadata keeps the existing trusted CPU frame ABI. */
static uint16_t width(uint16_t id) {
    Symbol *s=&cc.symbols[id];
    if(s->owner!=cc.scope) return 0;
    if(s->role==S_REFERENCE)return 2;
    if(s->role!=S_VARIABLE&&s->role!=S_VALUE&&s->role!=S_RESULT)return 0;
    return cc.types[s->type].width;
}
static uint16_t workspace(void) {
    uint16_t id,bytes=0;
    if(!(cc_rget(cc.scope,R_FLAGS)&RF_RECURSIVE))return 0;
    for(id=0;id<cc.symbol_count;++id)bytes+=width(id);
    return bytes;
}
static void block(uint16_t address,uint16_t bytes,uint8_t restoring) {
    cc_op16(restoring?0x11:0x21,address);
    cc_op16(0x01,bytes);cc_emit(0xed);cc_emit(0xb0);
}
static void copy(uint8_t restoring) {
    uint16_t id,n,start=0,bytes=0;
    for(id=0;id<cc.symbol_count;++id) {
        n=width(id);if(!n)continue;
        if(bytes&&cc.symbols[id].address!=start+bytes) {
            block(start,bytes,restoring);bytes=0;
        }
        if(!bytes)start=cc.symbols[id].address;
        bytes+=n;
    }
    if(bytes)block(start,bytes,restoring);
}
void cc_recursion_save(void) ROUTINES {
    uint16_t bytes=workspace(),active;
    if(!bytes||cc.error->code)return;
    cc_op16(0x2a,RT_RECURSION_CEILING);cc_emit(0x7c);cc_emit(0xb5);
    active=cc_forward(0xc2);
    cc_op16(0x2a,RT_RECURPTR);cc_op16(0x22,RT_RECURSION_CEILING);
    cc_patch(active);cc_guard();
    cc_op16(0x2a,RT_RECURPTR);cc_op16(0x11,bytes);
    cc_emit(0xb7);cc_emit(0xed);cc_emit(0x52);cc_op16(0x22,RT_RECURPTR);
    cc_guard();cc_emit(0xeb);copy(0);
}
void cc_recursion_restore(void) ROUTINES {
    uint16_t bytes=workspace(),overflow,valid,active;
    if(!bytes||cc.error->code)return;
    cc_guard();cc_emit(0xe5); /* scalar/String result survives the block copy */
    cc_op16(0x2a,RT_RECURPTR);cc_op16(0x11,bytes);cc_emit(0x19);
    overflow=cc_forward(0xda);
    cc_emit(0xed);cc_op16(0x5b,RT_RECURSION_CEILING);
    cc_emit(0xeb);cc_emit(0xb7);cc_emit(0xed);cc_emit(0x52);
    valid=cc_forward(0xd2);cc_patch(overflow);
    cc_emit(0x3e);cc_emit(0xff);cc_emit(0x06);cc_emit(2);cc_op16(0xc3,RT_ERROR);
    cc_patch(valid);cc_op16(0x2a,RT_RECURPTR);copy(1);cc_op16(0x22,RT_RECURPTR);
    cc_emit(0xed);cc_op16(0x5b,RT_RECURSION_CEILING);
    cc_emit(0xb7);cc_emit(0xed);cc_emit(0x52);active=cc_forward(0xc2);
    cc_op16(0x21,0);cc_op16(0x22,RT_RECURSION_CEILING);cc_patch(active);cc_emit(0xe1);
}
