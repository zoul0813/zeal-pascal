#include "compiler/internal.h"

/* Ordinary activation layout and returns, shared by routines and unit initializers. */
static void indexed(uint8_t opcode,uint8_t offset) { cc_emit(0xdd); cc_emit(opcode); cc_emit(offset); }
static void cursor_store(void) { indexed(0x75,250); indexed(0x74,251); }
static void cursor_load(void) { indexed(0x6e,250); indexed(0x66,251); }
static void raw_store(uint16_t id) {
    cc_guard(); cc_emit(0xe5); cc_slot(id); cc_emit(0xd1); cc_emit(0x73);
    if(CCS(id,role)==S_REFERENCE||CCT(CCS(id,type),width)==2) {
        cc_emit(0x23); cc_emit(0x72);
    }
}
void cc_frame_begin(void) ROUTINES {
    uint16_t id,first=cc_rget(cc.scope,R_PARAMETERS),count=cc_rget(cc.scope,R_COUNT);
    uint16_t bytes;
    uint16_t locals=cc_rget(cc.scope,R_LOCALS);
    cc_rput(cc.scope,R_LOCAL_GUARD,cc.emitted+3);
    cc_reserve(locals+FRAME_PRIVATE_BYTES+2); cc_emit(0xdd); cc_emit(0xe5); /* saved IX */
    cc_emit(0xdd); cc_op16(0x21,0); cc_emit(0xdd); cc_emit(0x39); /* IX = SP */
    cc_rput(cc.scope,R_LOCAL_ALLOC,cc.emitted+1);
    cc_op16(0x21,(uint16_t)(0u-FRAME_PRIVATE_BYTES-locals)); cc_emit(0x39); cc_emit(0xf9);
    indexed(0x75,248); indexed(0x74,249); /* actual owned-local lower bound */
    cc_frame(cc.scope); cc_op16(0x11,4); cc_emit(0x19); cursor_store();
    cc_recursion_save();
    while(count--&&!cc.error->code) {
        id=first+count; cursor_load();
        if(CCS(id,role)==S_VALUE&&(cc_family(CCS(id,type))==K_REAL||cc_family(CCS(id,type))==K_LONGWORD)) {
            bytes=CCT(CCS(id,type),width);
            cc_guard(); cc_emit(0xe5); cc_op16(0x11,bytes); cc_emit(0x19); cursor_store();
            cc_emit(0xe1); cc_destination(id); cc_op16(0x01,bytes); cc_call(RT_MOVE);
        } else if(CCS(id,role)==S_VALUE&&cc_family(CCS(id,type))==K_SET) {
            cc_guard(); cc_emit(0xe5); cc_op16(0x11,32); cc_emit(0x19); cursor_store();
            cc_emit(0xe1); cc_destination(id); cc_set_store(CCS(id,type));
        } else if(CCS(id,role)==S_VALUE&&cc_family(CCS(id,type))==K_STRING) {
            cc_guard(); cc_emit(0xe5);
            cc_emit(0x5e); cc_emit(0x16); cc_emit(0); cc_emit(0x13); cc_emit(0x19); cursor_store();
            cc_emit(0xe1); cc_destination(id);
            cc_emit(0x0e); cc_emit(CCT(CCS(id,type),capacity)); cc_call(RT_COPY);
        } else {
            cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0x23); cursor_store();
            cc_emit(0xeb);
            if(CCS(id,role)==S_VALUE&&cc_aggregate(CCS(id,type))) {
                cc_destination(id); cc_op16(0x01,CCT(CCS(id,type),width)); cc_call(RT_MOVE);
            } else raw_store(id);
        }
    }
    if(cc_rget(cc.scope,R_PARENT)) {
        cursor_load(); cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0x23); cursor_store(); cc_emit(0xeb);
    } else cc_op16(0x21,0);
    indexed(0x75,254); indexed(0x74,255); /* lexical parent */
    cc_frame(cc.scope); indexed(0x75,252); indexed(0x74,253); /* live frame lifetime */

}
void cc_frame_unwind(void) ROUTINES {
    indexed(0x6e,248); indexed(0x66,249); cc_emit(0xf9);
}
void cc_frame_end(void) ROUTINES {
    uint16_t i;
    uint16_t routine=cc_rget(cc.scope,R_SYMBOL);
    cc_frame_unwind();
    cc_guard(); /* source location for the checked return path */
    /* Exit already restored its stack and captured its own source/checking
     * location. Join after the scope-end guard. */
    cc_patch_chain(cc_rget(cc.scope,R_EXITS),cc.emitted);
    if(CCS(routine,role)==S_FUNCTION) {
        for(i=0;i<cc.symbol_count;++i)
            if(CCS(i,owner)==cc.scope&&CCS(i,role)==S_RESULT) { cc_load_symbol(i); break; }
        if(cc_family(CCS(routine,type))==K_STRING) {
            cc_guard(); cc_emit(0xe5); cursor_load(); cc_emit(0xeb); cc_emit(0xe1);
            cc_emit(0x0e); cc_emit(CCT(CCS(routine,type),capacity)); cc_call(RT_COPY);
            cursor_load(); /* caller-owned fixed result slot, returned in HL */
        } else if(cc_family(CCS(routine,type))==K_REAL||cc_family(CCS(routine,type))==K_LONGWORD) {
            cc_op16(0x11,RT_REAL_BUFFER); cc_op16(0x01,CCT(CCS(routine,type),width)); cc_call(RT_MOVE);
        }
    }
    cc_recursion_restore();
    cc_emit(0x44); cc_emit(0x4d); /* BC retains the scalar result */
    indexed(0x5e,2); indexed(0x56,3); /* original return word */
    cursor_load(); cc_emit(0xf9); /* caller's SP before arguments/link */
    indexed(0x6e,0); indexed(0x66,1);
    cc_emit(0xe5); cc_emit(0xdd); cc_emit(0xe1); /* restore saved IX */
    cc_emit(0x60); cc_emit(0x69); cc_emit(0xd5);
    if(CCS(routine,role)==S_FUNCTION&&cc_family(CCS(routine,type))==K_REAL) {
        cc_op16(0x2a,RT_REAL_BUFFER);
        cc_emit(0xed); cc_op16(0x5b,RT_REAL_BUFFER+2);
        cc_emit(0xed); cc_op16(0x4b,RT_REAL_BUFFER+4);
    }
    if(CCS(routine,role)==S_FUNCTION&&cc_family(CCS(routine,type))==K_LONGWORD)cc_op16(0x21,RT_REAL_BUFFER);
    cc_emit(0xc9);
}
