#include "compiler.h"

/* The compiler owns one workspace page. Relocations grow upward; these
 * sixteen-byte routine records grow downward. Only resident/scalar arguments
 * cross the compiler's bank boundaries. */
static uint8_t new_scope(uint16_t symbol) {
    uint8_t scope,i;
    if(cc.routine_count==255||(uint32_t)cc.relocations*2+(cc.routine_count+1u)*16>PAGE_SIZE) {
        cc_fail(98); return 0;
    }
    scope=(uint8_t)++cc.routine_count;
    for(i=0;i<16;++i) far_set(cc.workspace,PAGE_SIZE-(uint16_t)scope*16+i,0);
    cc_rput(scope,R_PARENT,cc.scope); cc_rput(scope,R_SYMBOL,symbol);
    cc_rput(scope,R_FLAGS,cc.absolute_mode?0:RF_RECURSIVE);
    return scope;
}
static void indexed(uint8_t opcode,uint8_t offset) { cc_emit(0xdd); cc_emit(opcode); cc_emit(offset); }
static void cursor_store(void) { indexed(0x75,250); indexed(0x74,251); }
static void cursor_load(void) { indexed(0x6e,250); indexed(0x66,251); }
static void raw_store(uint16_t id) {
    cc_guard(); cc_emit(0xe5); cc_slot(id); cc_emit(0xd1); cc_emit(0x73);
    if(cc.symbols[id].role==S_REFERENCE||cc.types[cc.symbols[id].type].width==2) {
        cc_emit(0x23); cc_emit(0x72);
    }
}
static void prologue(void) {
    uint16_t id,first=cc_rget(cc.scope,R_PARAMETERS),count=cc_rget(cc.scope,R_COUNT);
    uint16_t locals=cc_rget(cc.scope,R_LOCALS);
    cc_reserve(locals+8); cc_emit(0xdd); cc_emit(0xe5); /* saved IX */
    cc_emit(0xdd); cc_op16(0x21,0); cc_emit(0xdd); cc_emit(0x39); /* IX = SP */
    cc_op16(0x21,(uint16_t)(0u-6u-locals)); cc_emit(0x39); cc_emit(0xf9);
    cc_frame(cc.scope); cc_op16(0x11,4); cc_emit(0x19); cursor_store();
    while(count--&&!cc.error->code) {
        id=first+count; cursor_load();
        if(cc.symbols[id].role==S_VALUE&&cc_family(cc.symbols[id].type)==K_REAL) {
            cc_guard(); cc_emit(0xe5); cc_op16(0x11,6); cc_emit(0x19); cursor_store();
            cc_emit(0xe1); cc_destination(id); cc_op16(0x01,6); cc_call(RT_MOVE);
        } else if(cc.symbols[id].role==S_VALUE&&cc_family(cc.symbols[id].type)==K_SET) {
            cc_guard(); cc_emit(0xe5); cc_op16(0x11,32); cc_emit(0x19); cursor_store();
            cc_emit(0xe1); cc_destination(id); cc_set_store(cc.symbols[id].type);
        } else if(cc.symbols[id].role==S_VALUE&&cc_family(cc.symbols[id].type)==K_STRING) {
            cc_guard(); cc_emit(0xe5);
            cc_emit(0x5e); cc_emit(0x16); cc_emit(0); cc_emit(0x13); cc_emit(0x19); cursor_store();
            cc_emit(0xe1); cc_destination(id);
            cc_emit(0x0e); cc_emit(cc.types[cc.symbols[id].type].capacity); cc_call(RT_COPY);
        } else {
            cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0x23); cursor_store();
            cc_emit(0xeb);
            if(cc.symbols[id].role==S_VALUE&&cc_aggregate(cc.symbols[id].type)) {
                cc_destination(id); cc_op16(0x01,cc.types[cc.symbols[id].type].width); cc_call(RT_MOVE);
            } else raw_store(id);
        }
    }
    if(cc_rget(cc.scope,R_PARENT)) {
        cursor_load(); cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0x23); cursor_store(); cc_emit(0xeb);
    } else cc_op16(0x21,0);
    indexed(0x75,254); indexed(0x74,255); /* lexical parent */
    cc_frame(cc.scope); indexed(0x75,252); indexed(0x74,253); /* live frame lifetime */
}
static void epilogue(void) {
    uint16_t i;
    uint16_t routine=cc_rget(cc.scope,R_SYMBOL);
    if(cc.symbols[routine].role==S_FUNCTION) {
        for(i=0;i<cc.symbol_count;++i)
            if(cc.symbols[i].owner==cc.scope&&cc.symbols[i].role==S_RESULT) { cc_load_symbol(i); break; }
        if(cc_family(cc.symbols[routine].type)==K_STRING) {
            cc_guard(); cc_emit(0xe5); cursor_load(); cc_emit(0xeb); cc_emit(0xe1);
            cc_emit(0x0e); cc_emit(cc.types[cc.symbols[routine].type].capacity); cc_call(RT_COPY);
            cursor_load(); /* caller-owned fixed result slot, returned in HL */
        } else if(cc_family(cc.symbols[routine].type)==K_REAL) {
            cc_op16(0x11,RT_REAL_BUFFER); cc_op16(0x01,6); cc_call(RT_MOVE);
        }
    }
    cc_emit(0x44); cc_emit(0x4d); /* BC retains the scalar result */
    indexed(0x5e,2); indexed(0x56,3); /* original return word */
    cursor_load(); cc_emit(0xf9); /* caller's SP before arguments/link */
    indexed(0x6e,0); indexed(0x66,1);
    cc_emit(0xe5); cc_emit(0xdd); cc_emit(0xe1); /* restore saved IX */
    cc_emit(0x60); cc_emit(0x69); cc_emit(0xd5);
    if(cc.symbols[routine].role==S_FUNCTION&&cc_family(cc.symbols[routine].type)==K_REAL) {
        cc_op16(0x2a,RT_REAL_BUFFER);
        cc_emit(0xed); cc_op16(0x5b,RT_REAL_BUFFER+2);
        cc_emit(0xed); cc_op16(0x4b,RT_REAL_BUFFER+4);
    }
    cc_emit(0xc9);
}
static void parameters(uint8_t scope) {
    uint16_t first,count,i;
    int16_t type;
    uint8_t role;
    cc_rput(scope,R_PARAMETERS,cc.symbol_count);
    if(cc.token!='(') return;
    cc_next();
    do {
        role=cc_is("var")?S_REFERENCE:S_VALUE;
        if(role==S_REFERENCE) cc_next();
        first=cc.symbol_count;
        cc_new_symbol(role); cc_next();
        while(cc.token==','&&!cc.error->code) { cc_next(); cc_new_symbol(role); cc_next(); }
        count=cc.symbol_count-first;
        if(role==S_REFERENCE&&cc.token!=':') type=255;
        else {
            cc_expect(':',2); type=cc_named_type();
            if(type<0) cc_fail(41); else cc_next();
        }
        for(i=first;i<first+count&&!cc.error->code;++i) {
            cc.symbols[i].type=(uint8_t)type;
            cc_storage(i,role==S_REFERENCE?2:cc.types[type].width);
        }
        cc_rput(scope,R_COUNT,cc_rget(scope,R_COUNT)+count);
        if(cc.token!=';') break;
        cc_next();
    } while(!cc.error->code);
    cc_expect(')',5);
}
static void routine(void) {
    uint8_t role=cc_is("function")?S_FUNCTION:S_PROCEDURE,parent=cc.scope,scope,flags;
    uint16_t id,global_start=cc.image->globals_bytes,value;
    int16_t old,type;
    cc_next(); old=cc.token==T_ID?cc_lookup():-1;
    if(old>=0&&cc.symbols[old].owner==parent&&cc.symbols[old].role==role&&
       (cc_rget(cc.symbols[old].initial_type,R_FLAGS)&RF_FORWARD)&&
       !(cc_rget(cc.symbols[old].initial_type,R_FLAGS)&RF_DEFINED)) {
        id=(uint16_t)old; scope=cc.symbols[id].initial_type; cc_next(); cc_expect(';',1);
    } else {
        id=cc_new_symbol(role); scope=new_scope(id);
        if(cc.error->code) return;
        cc.symbols[id].initial_type=scope; cc_next(); cc.scope=scope;
        parameters(scope);
        if(role==S_FUNCTION) {
            cc_expect(':',2); type=cc_named_type();
            if(type<0||cc_aggregate((uint8_t)type)||cc_family((uint8_t)type)==K_SET) cc_fail(48);
            else {
                cc.symbols[id].type=(uint8_t)type;
                cc_result_symbol(id); cc_next();
            }
        }
        cc.scope=parent; cc_expect(';',1);
    }
    if(cc.error->code) return;
    flags=(uint8_t)cc_rget(scope,R_FLAGS);
    if(cc_is("forward")) { cc_rput(scope,R_FLAGS,flags|RF_FORWARD); cc_next(); cc_expect(';',1); return; }
    if(cc_is("external")) {
        cc_next(); cc_constant(&flags,&value);
        if(cc_family(flags)!=K_INTEGER) cc_fail(22);
        cc_rput(scope,R_ENTRY,value); cc_rput(scope,R_FLAGS,RF_DEFINED|RF_EXTERNAL);
        cc.image->globals_bytes=global_start;
        cc_expect(';',1);
        return;
    }
    cc_rput(scope,R_FLAGS,flags|RF_DEFINED); cc.scope=scope;
    cc_block(); cc.scope=parent; cc_expect(';',1);
}
void cc_block(void) ROUTINES {
    uint16_t first,jump=0,i,entry,start,code;
    uint8_t scope=cc.scope,saved_bound=cc.with_bound;
    if(++cc.nesting>24) { cc_fail(98); --cc.nesting; return; }
    while(!cc.error->code) {
        if(cc_is("label")) cc_labels();
        else if(cc_is("var")||cc_is("const")||cc_is("type")) {
            first=cc.symbol_count; cc_declarations(); cc_typed_storage(first);
        } else if(cc_is("procedure")||cc_is("function")) {
            if(!jump) jump=cc_forward(0xc3);
            routine();
        } else break;
    }
    for(i=1;i<=cc.routine_count&&!cc.error->code;++i)
        if(cc_rget((uint8_t)i,R_PARENT)==scope&&!(cc_rget((uint8_t)i,R_FLAGS)&RF_DEFINED)) cc_fail(73);
    if(jump) cc_patch(jump);
    if(scope) {
        entry=0x4000u+cc.emitted; cc_rput(scope,R_ENTRY,entry);
        cc.symbols[cc_rget(scope,R_SYMBOL)].address=entry;
        cc_patch_chain(cc_rget(scope,R_CALLS),entry); cc_rput(scope,R_CALLS,0);
        code=cc.emitted-0x8000u; start=cc.symbols[cc_rget(scope,R_SYMBOL)].name_start;
        prologue(); cc_mapping(code,cc.emitted-0x8000u,start,cc.token_start);
    }
    cc.with_bound=cc.with_limit;
    if(!cc_is("begin")) cc_fail(12); else cc_statement();
    cc_labels_finish();
    cc.with_bound=saved_bound;
    if(!cc.error->code&&cc.temporaries) cc_fail(99); /* no temporary escapes a statement/block */
    if(scope) {
        cc_patch_chain(cc_rget(scope,R_EXITS),0x4000u+cc.emitted);
        code=cc.emitted-0x8000u; start=cc.token_start;
        epilogue(); cc_mapping(code,cc.emitted-0x8000u,start,start);
    }
    --cc.nesting;
}
void cc_exit(void) ROUTINES {
    cc_with_unwind();
    if(!cc.scope) cc_op16(0xc3,RT_DONE);
    else cc_rput(cc.scope,R_EXITS,cc_link(0xc3,cc_rget(cc.scope,R_EXITS)));
}
static void entry_reference(uint8_t opcode,uint8_t scope) {
    uint16_t entry=cc_rget(scope,R_ENTRY);
    if(cc_rget(scope,R_FLAGS)&RF_EXTERNAL) cc_op16(opcode,entry);
    else if(entry) cc_address(opcode,entry);
    else cc_rput(scope,R_CALLS,cc_link(opcode,cc_rget(scope,R_CALLS)));
}
void cc_routine_address(uint16_t id) ROUTINES {
    uint8_t scope=cc.symbols[id].role==S_RESULT?cc.symbols[id].owner:cc.symbols[id].initial_type;
    if(cc_rget(scope,R_PARENT)) cc_fail(44); /* capturing/static bridges still pending */
    else entry_reference(0x21,scope);
}
uint8_t cc_invoke(uint16_t id) ROUTINES {
    uint8_t scope=cc.symbols[id].role==S_RESULT?cc.symbols[id].owner:cc.symbols[id].initial_type;
    uint16_t i,param,before,first=cc_rget(scope,R_PARAMETERS),count=cc_rget(scope,R_COUNT);
    uint16_t routine_id=cc_rget(scope,R_SYMBOL);
    uint8_t type,kind,role=cc.symbols[routine_id].role;
    cc_next();
    if(role==S_FUNCTION&&cc_family(cc.symbols[routine_id].type)==K_STRING)
        cc_temporary(cc.types[cc.symbols[routine_id].type].width);
    if(cc_rget(scope,R_PARENT)) { cc_frame((uint8_t)cc_rget(scope,R_PARENT)); cc_guard(); cc_emit(0xe5); }
    if(count||cc.token=='(') {
        cc_expect('(',4);
        for(i=0;i<count&&!cc.error->code;++i) {
            if(i) cc_expect(',',3);
            param=first+i; type=cc.symbols[param].type;
            if(cc.symbols[param].role==S_REFERENCE) {
                kind=cc_reference(44);
                if(type!=255&&type!=kind&&!(cc_family(type)==K_POINTER&&cc_compatible(type,kind))&&!(cc_family(type)==K_STRING&&cc_family(kind)==K_STRING&&
                   (!cc.var_strict||cc.types[type].capacity==cc.types[kind].capacity))) { cc_fail(44); break; }
                cc_guard(); cc_emit(0xe5);
            } else {
                before=cc.temporaries;
                kind=cc_aggregate(type)?cc_aggregate_actual(type):cc_expression(); cc_convert(type,kind);
                if(cc_family(type)==K_REAL) {
                    if(cc.temporaries-before!=6) cc_fail(99);
                    cc.temporaries=before; /* six bytes in public native storage order */
                } else if(cc_family(type)==K_SET) {
                    if(cc.temporaries-before!=32) cc_fail(99);
                    cc.temporaries=before; /* callee owns the full 32-byte value slot */
                } else if(cc_family(type)==K_STRING) {
                    if(cc_family(kind)==K_CHAR) { cc_guard(); cc_emit(0x65); cc_emit(0x2e); cc_emit(1); cc_emit(0xe5); }
                    else if(cc.temporaries!=before) {
                        cc_op16(0x01,cc.temporaries-before); cc_call(RT_ARGSTRING);
                        cc.temporaries=before;
                    }
                    else cc_call(RT_PUSHSTRING);
                } else {
                    cc_release(before);
                    if(cc.types[type].width==1&&!cc_aggregate(type)) { cc_emit(0x26); cc_emit(0); }
                    cc_guard(); cc_emit(0xe5);
                }
            }
        }
        cc_expect(')',5);
    }
    cc_guard(); entry_reference(0xcd,scope);
    if(role==S_FUNCTION&&cc_family(cc.symbols[routine_id].type)==K_STRING) {
        cc_op16(0x21,0); cc_emit(0x39); /* ABI result slot, independent of callee HL */
    } else if(role==S_FUNCTION&&cc_family(cc.symbols[routine_id].type)==K_REAL) {
        cc_reserve(6); cc_emit(0xc5); cc_emit(0xd5); cc_emit(0xe5);
        cc.temporaries+=6; cc_op16(0x21,0); cc_emit(0x39);
    }
    return role==S_FUNCTION?cc.symbols[routine_id].type:255;
}
