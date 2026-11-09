#include "units/internal.h"

static void expect_word(const char *name,uint8_t error) {
    if(cc_is(name))cc_next();else cc_fail(error);
}

/* Native Unit leaves use the ordinary scalar stack ABI, but own their raw
 * guard/frame/return instructions. No Pascal code may be inserted in an MMU
 * critical section. The declaration is contextual, not a reserved identifier. */
void cc_native_body(uint16_t symbol,uint16_t scope) ROUTINES {
    uint16_t entry=cc.emitted;
    uint8_t flags=(uint8_t)cc_rget(scope,R_FLAGS);
    if(!cc_native_signature(symbol,scope))return;
    flags=(flags|RF_DEFINED|RF_NATIVE)&~RF_RECURSIVE;
    cc_next();cc_expect(';',1);
    if(cc_is("callerio")){flags|=RF_CALLER_IO;cc_next();cc_expect(';',1);}
    if(cc_is("fullmemory")){cc_require_full_memory();cc_next();cc_expect(';',1);}
    if((flags&(RF_CALLER_IO|RF_REFERENCED))==(RF_CALLER_IO|RF_REFERENCED)) {
        cc_fail(UNIT_UNSUPPORTED);return;
    }
    cc_rput(scope,R_FLAGS,flags);
    /* Exports point directly at the native body. Earlier forward references
     * retain their patched thunk; unused declaration fragments drop at link. */
    cc_patch_chain(cc_rget(scope,R_CALLS),entry);cc_rput(scope,R_CALLS,0);
    cc_rput(scope,R_ENTRY,entry);
    expect_word("begin",12);
    while(!cc.error->code&&!cc_is("end")) {
        if(cc.token==';'){cc_next();continue;}
        if(!cc_is("inline")){cc_fail(UNIT_UNSUPPORTED);break;}
        cc_inline();
        if(cc.token!=';'&&!cc_is("end")){cc_fail(1);break;}
    }
    if(cc.emitted==entry&&!cc.error->code)cc_fail(UNIT_UNSUPPORTED);
    expect_word("end",13);cc_expect(';',1);
}

/* Native leaves use the scalar public stack ABI. */
uint8_t cc_native_signature(uint16_t symbol,uint16_t scope) ROUTINES {
    uint16_t i,param,type;
    if(CCS(symbol,role)==S_FUNCTION) {
        type=cc_family(CCS(symbol,type));
        if(type==K_STRING||type==K_REAL||type==K_LONGWORD){cc_fail(UNIT_UNSUPPORTED);return 0;}
    }
    for(i=0;i<cc_rget(scope,R_COUNT);++i) {
        param=cc_rget(scope,R_PARAMETERS)+i;type=CCS(param,type);
        if(CCS(param,role)!=S_REFERENCE&&(cc_aggregate(type)||cc_family(type)==K_STRING||
           cc_family(type)==K_SET||CCT(type,width)>2)){cc_fail(UNIT_UNSUPPORTED);return 0;}
    }
    return 1;
}
