#include "compiler_groups.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "compiler_units.h"
#endif

/* Routine metadata grows with the source and available compiler pages. */
static uint16_t new_scope(uint16_t symbol) {
    uint16_t scope;
    if(cc.routine_count==65535u) {cc_fail(99);return 0;}
    scope=++cc.routine_count;
    cc_routine_clear(scope);
    cc_rput(scope,R_PARENT,cc.scope); cc_rput(scope,R_SYMBOL,symbol);
    cc_rput(scope,R_FLAGS,cc.absolute_mode?0:RF_RECURSIVE);
    if(cc.sources->groups&&(scope>255u||!cc_groups_scope(cc.sources->groups,(uint8_t)scope))) {cc_fail(99);return 0;}
    return scope;
}
static void indexed(uint8_t opcode,uint8_t offset) { cc_emit(0xdd); cc_emit(opcode); cc_emit(offset); }
static void cursor_store(void) { indexed(0x75,250); indexed(0x74,251); }
static void cursor_load(void) { indexed(0x6e,250); indexed(0x66,251); }
static void raw_store(uint16_t id) {
    cc_guard(); cc_emit(0xe5); cc_slot(id); cc_emit(0xd1); cc_emit(0x73);
    if(CCS(id,role)==S_REFERENCE||CCT(CCS(id,type),width)==2) {
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
    cc_recursion_save();
    while(count--&&!cc.error->code) {
        id=first+count; cursor_load();
        if(CCS(id,role)==S_VALUE&&cc_family(CCS(id,type))==K_REAL) {
            cc_guard(); cc_emit(0xe5); cc_op16(0x11,6); cc_emit(0x19); cursor_store();
            cc_emit(0xe1); cc_destination(id); cc_op16(0x01,6); cc_call(RT_MOVE);
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
static void unwind_frame(void) {
    indexed(0x6e,248); indexed(0x66,249); cc_emit(0xf9);
}
static void epilogue(void) {
    uint16_t i;
    uint16_t routine=cc_rget(cc.scope,R_SYMBOL);
    unwind_frame();
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
        } else if(cc_family(CCS(routine,type))==K_REAL) {
            cc_op16(0x11,RT_REAL_BUFFER); cc_op16(0x01,6); cc_call(RT_MOVE);
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
    cc_emit(0xc9);
}
static void parameters(uint16_t scope) {
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
        if(role==S_REFERENCE&&cc.token!=':') type=-1;
        else {
            cc_expect(':',2); type=cc_named_type();
            if(type<0) cc_fail(41); else cc_next();
        }
        for(i=first;i<first+count&&!cc.error->code;++i) {
            if(role==S_VALUE&&type>=0&&cc_family((TypeId)type)==K_FILE) cc_fail(67);
            CCS_SET(i,type,(TypeId)type);
            cc_storage(i,role==S_REFERENCE?2:CCT(type,width));
        }
        cc_rput(scope,R_COUNT,cc_rget(scope,R_COUNT)+count);
        if(cc.token!=';') break;
        cc_next();
    } while(!cc.error->code);
    cc_expect(')',5);
}
#ifdef ZP_UNITS_EXPERIMENT
/* Existing forward declarations retain their parameter slots and names. A
 * repeated unit implementation heading validates those same declarations;
 * it never allocates a second, different workspace. */
static void repeated_heading(uint16_t id,uint16_t scope) {
    uint16_t first=cc_rget(scope,R_PARAMETERS),count=cc_rget(scope,R_COUNT),i=0,start,j;
    uint8_t role,k,c;TypeId type;SourcePosition name;
    if(cc.token=='(') {
        cc_next();
        do {
            role=cc_is("var")?S_REFERENCE:S_VALUE;if(role==S_REFERENCE)cc_next();start=i;
            for(;;) {
                if(i>=count||cc.token!=T_ID||CCS(first+i,role)!=role){cc_fail(UNIT_INTERFACE);return;}
                name=cc_reference_position(CCS(first+i,name_start));
                if(strlen(cc.name)!=CCS(first+i,name_length)){cc_fail(UNIT_INTERFACE);return;}
                for(k=0;k<CCS(first+i,name_length);++k) {
                    c=cc_source_byte(name+k);if(c>='A'&&c<='Z')c+=32;
                    if(c!=(uint8_t)cc.name[k]){cc_fail(UNIT_INTERFACE);return;}
                }
                ++i;cc_next();if(cc.token!=',')break;cc_next();
            }
            if(role==S_REFERENCE&&cc.token!=':')type=TYPE_NONE;
            else {cc_expect(':',2);type=(TypeId)cc_named_type();if(type==TYPE_NONE)cc_fail(UNIT_INTERFACE);cc_next();}
            for(j=start;j<i;++j)if(CCS(first+j,type)!=type)cc_fail(UNIT_INTERFACE);
            if(cc.token!=';')break;cc_next();
        } while(!cc.error->code);
        cc_expect(')',5);
        if(i!=count)cc_fail(UNIT_INTERFACE);
    } else if(count&&cc.token!=';')cc_fail(UNIT_INTERFACE);
    if(CCS(id,role)==S_FUNCTION&&cc.token==':') {
        cc_next();type=(TypeId)cc_named_type();
        if(type!=CCS(id,type))cc_fail(UNIT_INTERFACE);cc_next();
    }
}
#endif
static void routine(uint8_t interface_heading) {
    uint8_t role=cc_is("function")?S_FUNCTION:S_PROCEDURE;uint16_t parent=cc.scope;uint16_t scope;uint8_t flags;
    uint16_t id,global_start=cc.image->globals_bytes,value;
    SourcePosition position;
    int16_t old,type;TypeId actual;
    cc_next(); old=cc.token==T_ID?cc_lookup_decl():-1;
    if(old>=0&&CCS(old,owner)==parent&&CCS(old,role)==role&&
       (cc_rget(CCS(old,initial_type),R_FLAGS)&RF_FORWARD)&&
       !(cc_rget(CCS(old,initial_type),R_FLAGS)&RF_DEFINED)) {
        id=(uint16_t)old; scope=CCS(id,initial_type); cc_next();
#ifdef ZP_UNITS_EXPERIMENT
        if(cc.sources->unit_mode&&cc.token!=';')repeated_heading(id,scope);
#endif
        cc_expect(';',1);
    } else {
        id=cc_new_symbol(role); scope=new_scope(id);
        if(cc.error->code) return;
        CCS_SET(id,initial_type,scope); cc_next(); cc.scope=scope;
        parameters(scope);
        if(role==S_FUNCTION) {
            cc_expect(':',2); type=cc_named_type();
            if(type>=0&&cc_family((TypeId)type)==K_FILE) cc_fail(67);
            else if(type<0||cc_aggregate((TypeId)type)||cc_family((TypeId)type)==K_SET) cc_fail(48);
            else {
                CCS_SET(id,type,(TypeId)type);
                cc_result_symbol(id); cc_next();
            }
        }
        cc.scope=parent; cc_expect(';',1);
    }
    if(cc.error->code) return;
    flags=(uint8_t)cc_rget(scope,R_FLAGS);
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode&&cc_is("external")){cc_fail(UNIT_UNSUPPORTED);return;}
#endif
    if(cc_is("external")) {
        cc_require_real();
        if(!cc.suppress)cc.console_dynamic=1;
        if(flags&RF_FORWARD) {cc_fail(12);return;}
        cc_next(); cc_constant(&actual,&value);
        if(cc_family(actual)!=K_INTEGER) cc_fail(22);
        cc_rput(scope,R_ENTRY,value);cc_rput(scope,R_FLAGS,RF_DEFINED|RF_EXTERNAL);
        cc.image->globals_bytes=global_start;
        cc_expect(';',1);
        return;
    }
    if(!cc_rget(scope,R_ENTRY)) {
        /* An entry exists before nested bodies and forward definitions. Raw
         * Inline expressions can therefore use it without embedding fixup
         * lists in user instruction bytes or changing later Addr identities. */
#ifdef ZP_UNITS_EXPERIMENT
        if(cc.sources->unit_mode&&!parent)cc_units_owner(scope);
#endif
        CCS_SET(id,address,cc.emitted);cc_rput(scope,R_ENTRY,cc.emitted);
        cc_rput(scope,R_CALLS,cc_link(0xc3,0));
        position=cc_reference_position(CCS(id,name_start));
        cc_mapping(CCS(id,address)-CC_CODE_BASE,cc.emitted-CC_CODE_BASE,position,position);
#ifdef ZP_UNITS_EXPERIMENT
        if(cc.sources->unit_mode&&!parent)cc_units_owner(0);
#endif
    }
#ifdef ZP_UNITS_EXPERIMENT
    if(interface_heading) {
        cc_rput(scope,R_FLAGS,flags|RF_FORWARD);cc_units_export(id,scope);return;
    }
#else
    (void)interface_heading;
#endif
    if(cc_is("forward")) { cc_rput(scope,R_FLAGS,flags|RF_FORWARD); cc_next(); cc_expect(';',1); return; }
    cc_rput(scope,R_FLAGS,flags|RF_DEFINED); cc.scope=scope;
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode&&!parent)cc_units_owner(scope);
#endif
    cc_block(); cc.scope=parent;
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode&&!parent)cc_units_owner(0);
#endif
    cc_expect(';',1);
}
SourcePosition cc_block(void) ROUTINES {
    uint16_t first,jump=0,i,entry,code;
    SourcePosition start;
    uint16_t scope=cc.scope;uint8_t saved_bound=cc.with_bound;
    if(++cc.nesting==0||!cc_stack_check()) { cc_fail(99); --cc.nesting; return cc.token_start; }
    /* C selects CBreak's initial value at the program block's start. Later
     * source switches do not become assignments to this runtime variable. */
#ifdef ZP_UNITS_EXPERIMENT
    /* Initial CBreak belongs to final prefix placement. Deferring this one
     * root setting keeps the untouched template out of import/link peak RAM. */
    if(!scope&&!cc.sources->unit_mode)cc.sources->unit_cbreak=cc.sources->file?
        (cc.sources->parent_flags>>6)&1:cc.console_control;
#else
    if(!scope)cc_put(RT_CBREAK-0x4000u,cc.sources->file?
        (cc.sources->parent_flags>>6)&1:cc.console_control);
#endif
    while(!cc.error->code) {
        if(cc_is("label")) cc_labels();
        else if(cc_is("var")||cc_is("const")||cc_is("type")) {
            first=cc.symbol_count; cc_declarations(); cc_typed_storage(first);
        } else if(cc_is("procedure")||cc_is("function")) {
            if(!jump) jump=cc_forward(0xc3);
            routine(0);
        } else break;
    }
    for(i=1;i<=cc.routine_count&&!cc.error->code;++i)
        if(cc_rget(i,R_PARENT)==scope&&!(cc_rget(i,R_FLAGS)&RF_DEFINED)) cc_fail(73);
    if(jump) cc_patch(jump);
    if(scope) {
        entry=cc.emitted;
        cc_patch_chain(cc_rget(scope,R_CALLS),entry); cc_rput(scope,R_CALLS,0);
        code=cc.emitted-CC_CODE_BASE; start=cc_reference_position(CCS(cc_rget(scope,R_SYMBOL),name_start));
        prologue(); cc_mapping(code,cc.emitted-CC_CODE_BASE,start,cc.token_start);
    }
    cc.with_bound=cc.with_limit;
    start=cc.token_start;
    /* Direct Memory has no output owner for overlay group files. */
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode&&!scope) {
        if(!cc_is("end"))cc_fail(UNIT_UNSUPPORTED);else cc_next();
    } else
#endif
    if(!cc_is("begin")) cc_fail(cc.sources->memory&&cc_is("overlay")?77:12);
    else start=cc_statement();
    cc_labels_finish();
    cc.with_bound=saved_bound;
    if(!cc.error->code&&(cc.temporaries||cc.arguments)) cc_fail(99); /* no temporary escapes a statement/block */
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
        unwind_frame(); cc_guard();
        cc_rput(cc.scope,R_EXITS,cc_link(0xc3,cc_rget(cc.scope,R_EXITS)));
    }
}
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
                if(cc_family(type)==K_REAL) {
                    if(cc.temporaries-before!=6) cc_fail(99);
                    cc.temporaries=before; /* six bytes in public native storage order */
                    cc.arguments+=6;
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
                    /* Dynamic strings use at most 256 bytes. Imported units
                     * currently expose primitive parameters only. */
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

#ifdef ZP_UNITS_EXPERIMENT
void cc_units_interface(void) ROUTINES {
    while(!cc.error->code&&(cc_is("procedure")||cc_is("function")))routine(1);
    if(!cc_is("implementation")){cc_fail(UNIT_UNSUPPORTED);return;}
    cc_next();
}
#endif
