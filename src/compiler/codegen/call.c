#include "compiler/internal.h"
#ifdef __SDCC
#ifdef ZP_OVERLAYS
#pragma codeseg BANK2
#pragma constseg BANK2
#else
#pragma codeseg BANK5
#pragma constseg BANK5
#endif
#endif
TypeId cc_invoke(uint16_t id) ROUTINES {
    uint32_t arguments=cc.arguments;
    uint16_t scope=CCS(id,role)==S_RESULT?CCS(id,owner):CCS(id,initial_type);
    uint16_t i,param,before,first=cc_rget(scope,R_PARAMETERS),count=cc_rget(scope,R_COUNT);
    uint16_t routine_id=cc_rget(scope,R_SYMBOL);
    TypeId type;TypeId kind;uint8_t role=CCS(routine_id,role);
    cc_next();
    if(role==S_FUNCTION&&cc_family(CCS(routine_id,type))==K_STRING)
        cc_temporary(CCT(CCS(routine_id,type),width));
    if(cc_rget(scope,R_PARENT)) { cc_frame(cc_rget(scope,R_PARENT)); cc_guard(); cc_emit(0xe5);cc.arguments+=2; }
    if(count||cc.token=='(') {
        cc_expect('(',4);
        for(i=0;i<count&&!cc.error->code;++i) {
            if(i) cc_expect(',',3);
            param=first+i; type=CCS(param,type);
            if(CCS(param,role)==S_REFERENCE) {
                kind=cc_reference(44);
                if(type!=TYPE_NONE&&type!=kind&&!((cc_family(type)==K_POINTER||cc_family(type)==K_FILE)&&cc_compatible(type,kind))&&!(cc_family(type)==K_STRING&&cc_family(kind)==K_STRING&&
                   (!cc.var_strict||CCT(type,capacity)==CCT(kind,capacity)))) { cc_fail(44); break; }
                cc_guard(); cc_emit(0xe5);cc.arguments+=2;
            } else {
                before=cc.temporaries;
                kind=cc_aggregate(type)?cc_aggregate_actual(type):cc_expression(); cc_convert(type,kind);
                if(cc_family(type)==K_REAL||cc_family(type)==K_LONGWORD) {
                    if(cc.temporaries-before!=CCT(type,width)) cc_fail(99);
                    cc.temporaries=before; /* six bytes in public native storage order */
                    cc.arguments+=CCT(type,width);
                } else if(cc_family(type)==K_SET) {
                    if(cc.temporaries-before!=32) cc_fail(99);
                    cc.temporaries=before; /* callee owns the full 32-byte value slot */
                    cc.arguments+=32;
                } else if(cc_family(type)==K_STRING) {
                    if(cc_family(kind)==K_CHAR) { cc_guard(); cc_emit(0x65); cc_emit(0x2e); cc_emit(1); cc_emit(0xe5); }
                    else if(cc.temporaries!=before) {
                        cc_op16(0x01,cc.temporaries-before); cc_call(RT_ARGSTRING);
                        cc.temporaries=before;
                    }
                    else cc_call(RT_PUSHSTRING);
                    /* Dynamic strings, including imported arguments, use at most
                     * 256 bytes in the caller-owned value slot. */
                    cc.arguments+=cc_family(kind)==K_CHAR?2:256;
                } else {
                    cc_release(before);
                    if(CCT(type,width)==1&&!cc_aggregate(type)) { cc_emit(0x26); cc_emit(0); }
                    cc_guard(); cc_emit(0xe5);cc.arguments+=2;
                }
            }
        }
        cc_expect(')',5);
    }
    cc_guard(); cc_entry_reference(0xcd,scope);
    cc.arguments=arguments; /* Callee cleanup retires this call's arguments. */
    if(role==S_FUNCTION&&cc_family(CCS(routine_id,type))==K_STRING) {
        cc_op16(0x21,0); cc_emit(0x39); /* ABI result slot, independent of callee HL */
    } else if(role==S_FUNCTION&&cc_family(CCS(routine_id,type))==K_REAL) {
        cc_reserve(6); cc_emit(0xc5); cc_emit(0xd5); cc_emit(0xe5);
        cc.temporaries+=6; cc_op16(0x21,0); cc_emit(0x39);
    }
    return role==S_FUNCTION?CCS(routine_id,type):TYPE_NONE;
}
