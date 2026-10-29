#include "compiler.h"

/* The compiler owns one workspace page. Relocations grow upward; these
 * routine records grow downward. Only resident/scalar arguments
 * cross the compiler's bank boundaries. */
static uint8_t new_scope(uint16_t symbol) {
    uint8_t scope,i;
    if(cc.routine_count==255||(uint32_t)cc.relocations*RELOCATION_BYTES+(cc.routine_count+1u)*ROUTINE_BYTES>PAGE_SIZE) {
        cc_fail(98); return 0;
    }
    scope=(uint8_t)++cc.routine_count;
    for(i=0;i<ROUTINE_BYTES;++i) far_set(cc.workspace,PAGE_SIZE-(uint16_t)scope*ROUTINE_BYTES+i,0);
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
    cc_rput(cc.scope,R_LOCAL_GUARD,cc.emitted+3);
    cc_reserve(locals+FRAME_PRIVATE_BYTES+2); cc_emit(0xdd); cc_emit(0xe5); /* saved IX */
    cc_emit(0xdd); cc_op16(0x21,0); cc_emit(0xdd); cc_emit(0x39); /* IX = SP */
    cc_rput(cc.scope,R_LOCAL_ALLOC,cc.emitted+1);
    cc_op16(0x21,(uint16_t)(0u-FRAME_PRIVATE_BYTES-locals)); cc_emit(0x39); cc_emit(0xf9);
    indexed(0x75,248); indexed(0x74,249); /* actual owned-local lower bound */
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
    if(cc_rget(cc.scope,R_FLAGS)&RF_RECURSIVE)
        for(id=0;id<cc.symbol_count&&!cc.error->code;++id)
            if(cc.symbols[id].owner==cc.scope&&cc.symbols[id].role==S_VARIABLE&&cc_file_fields(cc.symbols[id].type)) {
                cc_symbol_address(id); cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xd1);
                cc_file_initialize(cc.symbols[id].type);
            }
}
static void unwind_frame(void) {
    indexed(0x6e,248); indexed(0x66,249); cc_emit(0xf9);
}
static void epilogue(void) {
    uint16_t i;
    uint16_t routine=cc_rget(cc.scope,R_SYMBOL);
    unwind_frame();
    cc_guard(); /* implicit checked close faults belong to this scope end */
    cc_emit(0x06); cc_emit(cc.io_check);
    /* Exit already restored its stack and captured its own source/checking
     * policy. Join after the implicit scope-end guard, before any cleanup. */
    cc_patch_chain(cc_rget(cc.scope,R_EXITS),cc.emitted);
    cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xd1); /* actual ending activation */
    cc_op16(0x21,0); cc_emit(0x3e); cc_emit(16);
    cc_op16(0xcd,RT_FILECLEANUP);
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
            if(role==S_VALUE&&type>=0&&cc_family((uint8_t)type)==K_FILE) cc_fail(67);
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
    SourcePosition position;
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
            if(type>=0&&cc_family((uint8_t)type)==K_FILE) cc_fail(67);
            else if(type<0||cc_aggregate((uint8_t)type)||cc_family((uint8_t)type)==K_SET) cc_fail(48);
            else {
                cc.symbols[id].type=(uint8_t)type;
                cc_result_symbol(id); cc_next();
            }
        }
        cc.scope=parent; cc_expect(';',1);
    }
    if(cc.error->code) return;
    flags=(uint8_t)cc_rget(scope,R_FLAGS);
    if(cc_is("external")) {
        if(flags&RF_FORWARD) {cc_fail(12);return;}
        cc_next(); cc_constant(&flags,&value);
        if(cc_family(flags)!=K_INTEGER) cc_fail(22);
        cc_rput(scope,R_ENTRY,value);cc_rput(scope,R_FLAGS,RF_DEFINED|RF_EXTERNAL);
        cc.image->globals_bytes=global_start;
        cc_expect(';',1);
        return;
    }
    if(!cc_rget(scope,R_ENTRY)) {
        /* An entry exists before nested bodies and forward definitions. Raw
         * Inline expressions can therefore use it without embedding fixup
         * lists in user instruction bytes or changing later Addr identities. */
        cc.symbols[id].address=cc.emitted;cc_rput(scope,R_ENTRY,cc.emitted);
        cc_rput(scope,R_CALLS,cc_link(0xc3,0));
        position=cc_reference_position(cc.symbols[id].name_start);
        cc_mapping(cc.symbols[id].address-CC_CODE_BASE,cc.emitted-CC_CODE_BASE,position,position);
    }
    if(cc_is("forward")) { cc_rput(scope,R_FLAGS,flags|RF_FORWARD); cc_next(); cc_expect(';',1); return; }
    cc_rput(scope,R_FLAGS,flags|RF_DEFINED); cc.scope=scope;
    cc_block(); cc.scope=parent; cc_expect(';',1);
}
SourcePosition cc_block(void) ROUTINES {
    uint16_t first,jump=0,i,entry,code;
    SourcePosition start;
    uint8_t scope=cc.scope,saved_bound=cc.with_bound;
    if(++cc.nesting>24) { cc_fail(98); --cc.nesting; return cc.token_start; }
    /* C selects CBreak's initial value at the program block's start. Later
     * source switches do not become assignments to this runtime variable. */
    if(!scope)cc_put(RT_CBREAK-0x4000u,cc.sources->file?
        (cc.sources->parent_flags>>6)&1:cc.console_control);
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
        entry=cc.emitted;
        cc_patch_chain(cc_rget(scope,R_CALLS),entry); cc_rput(scope,R_CALLS,0);
        code=cc.emitted-CC_CODE_BASE; start=cc_reference_position(cc.symbols[cc_rget(scope,R_SYMBOL)].name_start);
        prologue(); cc_mapping(code,cc.emitted-CC_CODE_BASE,start,cc.token_start);
    }
    cc.with_bound=cc.with_limit;
    start=cc.token_start;
    /* Direct Memory has no output owner for overlay group files. */
    if(!cc_is("begin")) cc_fail(!cc.sources->options&&cc_is("overlay")?77:12);
    else start=cc_statement();
    cc_labels_finish();
    cc.with_bound=saved_bound;
    if(!cc.error->code&&cc.temporaries) cc_fail(99); /* no temporary escapes a statement/block */
    if(scope) {
        code=cc.emitted-CC_CODE_BASE;
        epilogue(); cc_mapping(code,cc.emitted-CC_CODE_BASE,start,start);
        /* Addr may discover a declaring activation's private thunk slot after
         * its prologue was emitted. Existing local offsets remain unchanged. */
        cc_word(cc_rget(scope,R_LOCAL_GUARD),cc_rget(scope,R_LOCALS)+FRAME_PRIVATE_BYTES+2);
        cc_word(cc_rget(scope,R_LOCAL_ALLOC),(uint16_t)(0u-FRAME_PRIVATE_BYTES-cc_rget(scope,R_LOCALS)));
    }
    --cc.nesting;
    return start;
}
void cc_exit(void) ROUTINES {
    cc_with_unwind();
    if(!cc.scope) cc_finish(RT_DONE);
    else {
        unwind_frame(); cc_guard(); cc_emit(0x06); cc_emit(cc.io_check);
        cc_rput(cc.scope,R_EXITS,cc_link(0xc3,cc_rget(cc.scope,R_EXITS)));
    }
}
static void entry_reference(uint8_t opcode,uint8_t scope) {
    uint16_t entry=cc_rget(scope,R_ENTRY);
    if(cc_rget(scope,R_FLAGS)&RF_EXTERNAL) cc_op16(opcode,entry);
    else if(entry) cc_address(opcode,entry);
    else cc_rput(scope,R_CALLS,cc_link(opcode,cc_rget(scope,R_CALLS)));
}
/* A bridge is a real dynamic frame for file-owner traversal. The published
 * first eight bytes keep their ordinary meaning; the remaining private words
 * retain IY, BCDEHL results and the original callback caller's source PC. */
#define BRIDGE_PRIVATE_BYTES 18u
static void bridge_guard(uint16_t bytes) {
    uint16_t at=cc.emitted;
    /* Only A/flags are scratch here. Preserve operand registers and the actual
     * caller PC instead of attributing a bridge overflow to an earlier Addr. */
    cc_op16(0x3a,RT_PC); cc_address(0x32,at+17);
    cc_op16(0x3a,RT_PC+1); cc_address(0x32,at+18);
    cc_reserve(bytes);
}
static void bridge_slot(int16_t offset) {
    cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xe1);
    cc_op16(0x11,(uint16_t)offset); cc_emit(0x19);
}
static uint16_t argument_bytes(uint16_t id) {
    uint8_t type=cc.symbols[id].type;
    if(cc.symbols[id].role==S_REFERENCE) return 2;
    if(cc_family(type)==K_REAL) return 6;
    if(cc_family(type)==K_SET) return 32;
    return 2; /* scalars, pointers and aggregate source addresses */
}
static void bridge_body(uint8_t scope) {
    uint16_t count=cc_rget(scope,R_COUNT),first=cc_rget(scope,R_PARAMETERS),i,width;
    uint16_t routine=cc_rget(scope,R_SYMBOL);
    uint8_t string_result=cc.symbols[routine].role==S_FUNCTION&&cc_family(cc.symbols[routine].type)==K_STRING;
    width=BRIDGE_PRIVATE_BYTES+count*2;
    bridge_guard(width+4); cc_emit(0xeb); /* DE owns the descriptor */
    cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xdd); cc_op16(0x21,0); cc_emit(0xdd); cc_emit(0x39);
    cc_op16(0x21,(uint16_t)(0u-width)); cc_emit(0x39); cc_emit(0xf9);
    indexed(0x75,248); indexed(0x74,249);
    cc_emit(0xeb); cc_op16(0x01,10); cc_emit(0x09);
    cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56);
    indexed(0x73,254); indexed(0x72,255);
    cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xe1); indexed(0x75,252); indexed(0x74,253);
    cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe1); indexed(0x75,246); indexed(0x74,247);
    cc_op16(0x2a,RT_PC); indexed(0x75,238); indexed(0x74,239);
    bridge_slot(4); cc_emit(0xe5); cc_emit(0xfd); cc_emit(0xe1); /* IY scans original actuals */
    for(i=count;i&&!cc.error->code;--i) {
        bridge_slot((int16_t)(0u-BRIDGE_PRIVATE_BYTES-i*2));
        cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xd1);
        cc_emit(0x73); cc_emit(0x23); cc_emit(0x72);
        if(cc.symbols[first+i-1].role==S_VALUE&&cc_family(cc.symbols[first+i-1].type)==K_STRING) {
            cc_emit(0xfd); cc_emit(0x5e); cc_emit(0); cc_emit(0x16); cc_emit(0); cc_emit(0x13);
        } else cc_op16(0x11,argument_bytes(first+i-1));
        cc_emit(0xfd); cc_emit(0x19);
    }
    cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe1); cursor_store();
    if(string_result) {
        width=cc.types[cc.symbols[routine].type].width;
        bridge_guard(width); cc_op16(0x21,(uint16_t)(0u-width)); cc_emit(0x39); cc_emit(0xf9);
    }
    indexed(0x6e,254); indexed(0x66,255); bridge_guard(2); cc_emit(0xe5);
    for(i=0;i<count&&!cc.error->code;++i) {
        bridge_guard(2); bridge_slot((int16_t)(0u-BRIDGE_PRIVATE_BYTES-(i+1u)*2));
        cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
        if(cc.symbols[first+i].role==S_VALUE&&cc_family(cc.symbols[first+i].type)==K_STRING) {
            bridge_guard(257); cc_op16(0xcd,RT_PUSHSTRING);
        } else {
            width=argument_bytes(first+i); bridge_guard(width+2);
            cc_emit(0xeb); cc_op16(0x21,(uint16_t)(0u-width)); cc_emit(0x39); cc_emit(0xf9); cc_emit(0xeb);
            cc_op16(0x01,width); cc_op16(0xcd,RT_MOVE);
        }
    }
    bridge_guard(2); entry_reference(0xcd,scope);
    indexed(0x75,244); indexed(0x74,245); /* result HL */
    indexed(0x73,242); indexed(0x72,243); /* result DE */
    indexed(0x71,240); indexed(0x70,241); /* result BC */
    indexed(0x6e,238); indexed(0x66,239); cc_op16(0x22,RT_PC);
    if(string_result) {
        cc_op16(0x21,0); cc_emit(0x39); indexed(0x5e,250); indexed(0x56,251);
        cc_emit(0x0e); cc_emit(cc.types[cc.symbols[routine].type].capacity);
        bridge_guard(2); cc_op16(0xcd,RT_COPY);
        cursor_load(); indexed(0x75,244); indexed(0x74,245);
    }
    /* Build a return packet below our own frame, then memmove it into retired
     * original actual slots. Original arguments survive until the callee has
     * finished. IY holds the destination while the old frame is overwritten. */
    bridge_guard(14);
    indexed(0x6e,2); indexed(0x66,3); cc_emit(0xe5);
    indexed(0x6e,0); indexed(0x66,1); cc_emit(0xe5);
    indexed(0x6e,246); indexed(0x66,247); cc_emit(0xe5);
    indexed(0x6e,240); indexed(0x66,241); cc_emit(0xe5);
    indexed(0x6e,242); indexed(0x66,243); cc_emit(0xe5);
    indexed(0x6e,244); indexed(0x66,245); cc_emit(0xe5);
    cursor_load(); cc_op16(0x11,(uint16_t)-12); cc_emit(0x19);
    cc_emit(0xe5); cc_emit(0xfd); cc_emit(0xe1); cc_emit(0xeb);
    cc_op16(0x21,0); cc_emit(0x39); cc_op16(0x01,12); cc_op16(0xcd,RT_MOVE);
    cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe1); cc_emit(0xf9);
    cc_emit(0xe1); cc_emit(0xd1); cc_emit(0xc1);
    cc_emit(0xfd); cc_emit(0xe1); cc_emit(0xdd); cc_emit(0xe1); cc_emit(0xc9);
}
static uint16_t bridge_descriptor(uint8_t scope,uint8_t dynamic) {
    uint16_t skip=cc_forward(0xc3),entry=cc.emitted,descriptor,i;
    bridge_body(scope); descriptor=cc.emitted;
    cc_emit('Z'); cc_emit('T'); cc_emit('H'); cc_emit('K'); cc_emit(1); cc_emit(dynamic);
    cc_emit(16); cc_emit(0);
    cc_relocate(cc.emitted); cc_emit((uint8_t)entry); cc_emit((uint8_t)(entry>>8));
    for(i=0;i<6;++i) cc_emit(0); /* lexical frame, lifetime, reserved */
    if(dynamic) cc_op16(0x21,0); else cc_address(0x21,descriptor);
    cc_address(0xc3,entry); cc_patch(skip);
    return descriptor;
}
void cc_routine_address(uint16_t id) ROUTINES {
    uint8_t scope=cc.symbols[id].role==S_RESULT?cc.symbols[id].owner:cc.symbols[id].initial_type;
    uint8_t parent=(uint8_t)cc_rget(scope,R_PARENT),dynamic;
    uint16_t descriptor,slot,locals;
    if(!parent) { entry_reference(0x21,scope); return; }
    if(cc.suppress||cc.error->code) return;
    dynamic=cc_frame_required(parent);
    descriptor=cc_rget(scope,R_DESCRIPTOR);
    if(!descriptor) {
        descriptor=bridge_descriptor(scope,dynamic); cc_rput(scope,R_DESCRIPTOR,descriptor);
    }
    if(!dynamic) { cc_address(0x21,descriptor+16); return; }
    slot=cc_rget(scope,R_THUNK);
    if(!slot) {
        locals=cc_rget(parent,R_LOCALS);
        if((uint32_t)locals+22>0x7ff0u) { cc_fail(99); return; }
        locals+=22; cc_rput(parent,R_LOCALS,locals);
        slot=(uint16_t)(0u-FRAME_PRIVATE_BYTES-locals); cc_rput(scope,R_THUNK,slot);
    }
    cc_frame(parent); cc_reserve(6); cc_emit(0xe5); /* live declaring frame */
    cc_op16(0x11,slot); cc_emit(0x19); cc_emit(0xe5); cc_emit(0xeb);
    cc_address(0x21,descriptor); cc_op16(0x01,22); cc_call(RT_MOVE);
    cc_emit(0xe1); cc_emit(0xd1); cc_emit(0xe5); /* descriptor, owner, saved descriptor */
    cc_op16(0x01,10); cc_emit(0x09);
    cc_emit(0x73); cc_emit(0x23); cc_emit(0x72); cc_emit(0x23);
    cc_emit(0x73); cc_emit(0x23); cc_emit(0x72); /* lifetime is the live frame's ID */
    cc_op16(0x01,4); cc_emit(0x09); cc_emit(0xd1);
    cc_emit(0x73); cc_emit(0x23); cc_emit(0x72); /* LD HL,descriptor operand */
    cc_emit(0xeb); cc_op16(0x01,16); cc_emit(0x09);
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
                if(type!=255&&type!=kind&&!((cc_family(type)==K_POINTER||cc_family(type)==K_FILE)&&cc_compatible(type,kind))&&!(cc_family(type)==K_STRING&&cc_family(kind)==K_STRING&&
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
