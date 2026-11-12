#include "compiler/groups/groups.h"
#include "compiler/groups/native.h"
#include "platform/banks/loader.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "units/internal.h"
#endif

/* Routine metadata grows with the source and available compiler pages. */
static uint16_t new_scope(uint16_t symbol) {
    uint16_t scope;
    if(cc.routine_count==65535u) {cc_fail(99);return 0;}
    scope=++cc.routine_count;
    cc_routine_clear(scope);
    cc_rput(scope,R_PARENT,cc.scope); cc_rput(scope,R_SYMBOL,symbol);
    cc_rput(scope,R_FLAGS,cc.absolute_mode?0:RF_RECURSIVE);
    #ifdef ZP_OVERLAYS
    if(cc.sources->overlays)cc_rput(scope,R_BANK,cc.sources->bank);
    else
    #endif
    if(cc.sources->groups&&(scope>255u||!cc_groups_scope(cc.sources->groups,(uint8_t)scope))) {cc_fail(99);return 0;}
    return scope;
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
static void routine(uint8_t interface_heading,uint8_t overlay) {
    uint8_t role=cc_is("function")?S_FUNCTION:S_PROCEDURE;uint16_t parent=cc.scope;uint16_t scope;uint8_t flags;
#ifdef ZP_OVERLAYS
    uint16_t bank=0,parent_bank=0;
#endif
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
        if(!cc.suppress)cc.console_dynamic=1;
        if(flags&RF_FORWARD) {cc_fail(12);return;}
        cc_next(); cc_constant(&actual,&value);
        if(cc_family(actual)!=K_INTEGER) cc_fail(22);
        cc_rput(scope,R_ENTRY,value);cc_rput(scope,R_FLAGS,RF_DEFINED|RF_EXTERNAL);
        cc.image->globals_bytes=global_start;
        cc_expect(';',1);
        return;
    }
    #ifdef ZP_OVERLAYS
    if(cc.sources->overlays) {
        if(!cc_rget(scope,R_BANK)&&!cc_rget(scope,R_ENTRY))cc_overlay_alias(scope);
    }
    else
    #endif
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
    if(interface_heading&&!overlay) {
        cc_rput(scope,R_FLAGS,flags|RF_FORWARD);cc_units_export(id,scope);return;
    }
#else
    (void)interface_heading;
#endif
    if(overlay&&cc_is("forward")){cc_fail(12);return;}
    if(cc_is("forward")) { cc_rput(scope,R_FLAGS,flags|RF_FORWARD); cc_next(); cc_expect(';',1); return; }
#ifdef ZP_UNITS_EXPERIMENT
    if(cc_is("native")) {
        if(!cc.sources->unit_mode||parent){cc_fail(UNIT_UNSUPPORTED);return;}
        cc.scope=scope;cc_units_owner(scope);
        cc_native_body(id,scope);
        cc.scope=parent;cc_units_owner(0);return;
    }
#endif
    #ifdef ZP_OVERLAYS
    if(overlay){parent_bank=cc.sources->bank;bank=cc_overlay_member(interface_heading,scope);}
    #endif
    cc_rput(scope,R_FLAGS,flags|RF_DEFINED); cc.scope=scope;
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode&&!parent)cc_units_owner(scope);
#endif
    cc_block(); cc.scope=parent;
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode&&!parent)cc_units_owner(0);
#endif
    cc_expect(';',1);
#ifdef ZP_OVERLAYS
    if(overlay&&!cc.error->code){cc_overlay_finish_member(bank);cc_overlay_switch(parent_bank);cc_overlay_wrappers(bank);}
#endif
}
SourcePosition cc_block(void) ROUTINES {
    uint16_t first,jump=0,i,entry,code;uint8_t group=255;
    SourcePosition start;
    uint16_t scope=cc.scope;uint8_t saved_bound=cc.with_bound;
#ifdef ZP_UNITS_EXPERIMENT
    uint8_t initializer=0;
#endif
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
        if(cc_is("overlay")) {
#ifdef ZP_OVERLAYS
            if(cc.sources->memory){cc_fail(77);break;}
            if(!cc.sources->overlays){cc_fail(COMPILE_OVERLAY_RETRY);break;}
            if(cc.sources->unit_mode){cc_fail(UNIT_UNSUPPORTED);break;}
            if(!jump)jump=cc_forward(0xc3);
            if(group==255)group=cc_overlay_group();
            cc_next();if(!cc_is("procedure")&&!cc_is("function")){cc_fail(12);break;}
            routine(group,1);
#else
            cc_fail(cc.sources->memory?77:12);
#endif
        } else if(cc_is("label")) {group=255;cc_labels();}
        else if(cc_is("var")||cc_is("const")||cc_is("type")) {
            group=255;first=cc.symbol_count; cc_declarations(); cc_typed_storage(first);
        } else if(cc_is("procedure")||cc_is("function")) {
            if(!jump) jump=cc_forward(0xc3);
            group=255;routine(0,0);
        } else break;
    }
    for(i=1;i<=cc.routine_count&&!cc.error->code;++i)
        if(cc_rget(i,R_PARENT)==scope&&!(cc_rget(i,R_FLAGS)&RF_DEFINED)) cc_fail(73);
    if(jump) cc_patch(jump);
#ifdef ZP_UNITS_EXPERIMENT
    /* A syntactically empty body has no runtime root or frame. */
    if(cc.sources->unit_mode&&!scope&&cc_is("begin")) {
        SourceCursor cursor;cc_source_save(&cursor);cc_next();
        while(cc.token==';')cc_next();
        if(cc_is("end")){cc_next();initializer=2;}
        else cc_source_restore(&cursor);
    }
    /* An initializer is an anonymous ordinary procedure. Global declarations
     * retain scope0; only labels belong to this executable activation. */
    if(cc.sources->unit_mode&&!scope&&cc_is("begin")&&!cc.error->code) {
        if(cc.symbol_count==SYMBOL_LIMIT){cc_fail(99);}
        else {
            i=cc.symbol_count++;cc_symbol_clear(i);CCS_SET(i,role,S_PROCEDURE);
            CCS_SET(i,name_start,cc_source_reference(cc.token_start));
            scope=new_scope(i);CCS_SET(i,initial_type,scope);
            cc_rput(scope,R_FLAGS,cc_rget(scope,R_FLAGS)|RF_DEFINED);
            cc.scope=scope;initializer=1;
            for(i=0;i<cc.symbol_count;++i)
                if(CCS(i,role)==S_LABEL&&!CCS(i,owner))CCS_SET(i,owner,scope);
            cc_units_owner(scope);cc_rput(scope,R_ENTRY,cc.emitted);
            cc_unit_record(UR_INITIALIZER,cc.emitted,0,0,0,0);
        }
    }
#endif
    if(scope) {
        entry=cc.emitted;
#ifdef ZP_OVERLAYS
        if(cc_rget(scope,R_BANK))cc_rput(scope,R_BODY,entry);
        else
#endif
        {cc_patch_chain(cc_rget(scope,R_CALLS),entry); cc_rput(scope,R_CALLS,0);}
        code=cc.emitted-CC_CODE_BASE; start=cc_reference_position(CCS(cc_rget(scope,R_SYMBOL),name_start));
#ifdef ZP_OVERLAYS
        if(!cc_rget(scope,R_BANK))
#endif
        cc_frame_begin(); cc_mapping(code,cc.emitted-CC_CODE_BASE,start,cc.token_start);
    }
    cc.with_bound=cc.with_limit;
    start=cc.token_start;
    /* Direct Memory has no output owner for overlay group files. */
#ifdef ZP_UNITS_EXPERIMENT
    if(initializer==2) { /* Empty body consumed above. */ }
    else if(cc.sources->unit_mode&&!scope) {
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
#ifdef ZP_OVERLAYS
        if(cc_rget(scope,R_BANK)){cc_guard();cc_emit(0xc9);}else
#endif
        cc_frame_end(); cc_mapping(code,cc.emitted-CC_CODE_BASE,start,start);
        /* Addr may discover a declaring activation's private thunk slot after
         * its prologue was emitted. Existing local offsets remain unchanged. */
#ifdef ZP_OVERLAYS
        if(!cc_rget(scope,R_BANK)) {
#endif
        cc_word(cc_rget(scope,R_LOCAL_GUARD),cc_rget(scope,R_LOCALS)+FRAME_PRIVATE_BYTES+2);
        cc_word(cc_rget(scope,R_LOCAL_ALLOC),(uint16_t)(0u-FRAME_PRIVATE_BYTES-cc_rget(scope,R_LOCALS)));
#ifdef ZP_OVERLAYS
        }
#endif
    }
#ifdef ZP_UNITS_EXPERIMENT
    if(initializer==1){cc.scope=0;cc_units_owner(0);}
#endif
    (void)group;
    --cc.nesting;
    return start;
}
void cc_exit(void) ROUTINES {
    cc_with_unwind();
    if(!cc.scope) cc_finish(RT_DONE);
    else {
#ifdef ZP_OVERLAYS
        if(cc_rget(cc.scope,R_BANK)) {
            cc_guard();cc_frame_unwind();cc_emit(0x3b);cc_emit(0x3b);cc_emit(0x3b);cc_emit(0x3b);cc_emit(0xc9);return;
        }
#endif
        cc_frame_unwind(); cc_guard();
        cc_rput(cc.scope,R_EXITS,cc_link(0xc3,cc_rget(cc.scope,R_EXITS)));
    }
}

#ifdef ZP_UNITS_EXPERIMENT
void cc_units_interface(void) ROUTINES {
    uint16_t first,i;
    if(cc_is("uses"))cc_units_imports();
    while(!cc.error->code) {
        if(cc_is("procedure")||cc_is("function"))routine(1,0);
        else if(cc_is("const")||cc_is("type")) {
            first=cc.symbol_count;cc_declarations();
            for(i=first;i<cc.symbol_count&&!cc.error->code;++i)if(CCS(i,owner)==0&&CCS(i,name_length)) {
                if(CCS(i,role)!=S_CONSTANT&&CCS(i,role)!=S_TYPE)cc_fail(UNIT_UNSUPPORTED);
                else cc_unit_record(UR_EXPORT,i,0,0,0,0);
            }
        } else break;
    }
    if(!cc_is("implementation")){cc_fail(UNIT_UNSUPPORTED);return;}
    cc_next();if(cc_is("uses"))cc_units_imports();
}
#endif
