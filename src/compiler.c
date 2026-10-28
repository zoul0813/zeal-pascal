#ifdef __SDCC
#define compile compiler_compile
#define compile_project compiler_compile_project
#endif
#include "compiler.h"


static uint16_t pc(void) { return cc.emitted-CC_CODE_BASE; }

static uint16_t condition(void) {
    cc_user_interrupt();
    if(cc_family(cc_expression())!=K_BOOLEAN) cc_fail(20);
    cc_emit(0x7c); cc_emit(0xb5); /* A = H | L; false has Z set */
    return cc_forward(0xca);
}
static SourcePosition statement(void);
/* Labels share the lexical namespace, but With fields never capture a Goto.
 * Numeric names use their source spelling, rather than an Integer value. */
static int16_t label_lookup(void) {
    uint8_t saved=cc.with_depth;
    int16_t id;
    if(cc.token!=T_ID&&(cc.token!=T_NUMBER||cc.hexadecimal)) return -1;
    cc.with_depth=0; id=cc_lookup(); cc.with_depth=saved;
    return id;
}
void cc_labels(void) BACKEND {
    uint16_t token;
    cc_next();
    do {
        if(cc.token!=T_ID&&(cc.token!=T_NUMBER||cc.hexadecimal)) { cc_fail(cc.hexadecimal?58:53); return; }
        token=cc.token; cc.token=T_ID; cc_new_symbol(S_LABEL); cc.token=token;
        cc_next();
        if(cc.token!=',') break;
        cc_next();
    } while(!cc.error->code);
    cc_expect(';',1);
}
void cc_labels_finish(void) BACKEND {
    uint16_t i;
    for(i=0;i<cc.symbol_count&&!cc.error->code;++i)
        if(cc.symbols[i].role==S_LABEL&&cc.symbols[i].owner==cc.scope&&
           !cc.symbols[i].initial_type&&cc.symbols[i].address) cc_fail(40);
}
static uint16_t label_word(uint16_t at) {
    return cc_code_get(at)|((uint16_t)cc_code_get(at+1)<<8);
}
/* A pending jump keeps its source For/With depths in immediate operands.
 * The JP operand links the pending records until the label is defined.
 * All JP destinations use the ordinary code relocation ledger. */
static void label_patch(uint16_t id,uint16_t at) {
    Symbol *s=&cc.symbols[id];
    uint16_t loops=label_word(at-5);
    uint8_t with=cc_code_get(at-25);
    if(loops<s->type) { cc_fail(71); return; }
    cc_put(at-25,with>s->control?with-s->control:0);
    cc_word(at-5,(uint16_t)((loops-s->type)*2+(int16_t)(with-s->control)*4));
    cc_word(at,s->address);
}
static void label_define(uint16_t id) {
    Symbol *s=&cc.symbols[id];
    uint16_t head=s->address,next;
    if(s->owner!=cc.scope) { cc_fail(72); return; }
    if(s->initial_type) { cc_fail(43); return; }
    s->initial_type=1; s->address=cc.emitted;
    s->type=cc.for_depth; s->control=cc.with_depth;
    while(head&&!cc.error->code) {
        next=label_word(head); label_patch(id,head); head=next;
    }
    cc_next(); cc_expect(':',2);
}
static void goto_statement(void) {
    int16_t id;
    uint16_t at;
    cc_next(); id=label_lookup();
    if(id<0||cc.symbols[id].role!=S_LABEL) { cc_fail(40); return; }
    if(cc.symbols[id].owner!=cc.scope) { cc_fail(72); return; }
    cc_guard();
    cc_emit(0x06); cc_emit(cc.with_depth); /* B = With frames to leave */
    cc_emit(0x78); cc_emit(0xb7); cc_emit(0x28); cc_emit(14);
    cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe1); /* HL = current With frame */
    cc_emit(0x23); cc_emit(0x23); cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
    cc_emit(0x10); cc_emit(248); /* follow saved IY links */
    cc_emit(0xe5); cc_emit(0xfd); cc_emit(0xe1);
    cc_op16(0x21,cc.for_depth); cc_emit(0x39); cc_emit(0xf9);
    at=cc_link(0xc3,cc.symbols[id].initial_type?0:cc.symbols[id].address);
    if(cc.symbols[id].initial_type) label_patch((uint16_t)id,at);
    else cc.symbols[id].address=at;
    cc_next();
}
static void with_statement(SourcePosition start,uint16_t code) {
    uint8_t saved=cc.with_depth,type;
    int16_t id;
    cc_next();
    do {
        if(cc.with_depth>=cc.with_bound||cc.with_depth==9) { cc_fail(97); break; }
        id=cc.token==T_ID?cc_lookup():-1;
        if(!cc_variable(id)) { cc_fail(41); break; }
        type=cc_selection((uint16_t)id);
        if(cc_family(type)!=K_RECORD) { cc_fail(29); break; }
        cc_reserve(4); cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe5);
        cc_emit(0xfd); cc_op16(0x21,0); cc_emit(0xfd); cc_emit(0x39);
        cc.with_types[cc.with_depth++]=type;
        if(cc.token!=',') break; cc_next();
    } while(!cc.error->code);
    cc_keyword("do",13); cc_mapping(code,pc(),start,cc.token_start);
    statement(); code=pc();
    while(cc.with_depth>saved) {
        cc_emit(0xe1); cc_emit(0xfd); cc_emit(0xe1); --cc.with_depth;
    }
    cc_mapping(code,pc(),start,cc.token_start);
}
/* Forward operands double as a private patch list until the image is sealed.
 * Each nested Case owns its own heads; no extra compiler workspace is needed. */
static void link_jump(uint8_t opcode,uint16_t *head) {
    *head=cc_link(opcode,*head);
}
static void patch_jumps(uint16_t head) {
    cc_patch_chain(head,cc.emitted);
}
static uint16_t case_constant(uint8_t selector) {
    uint8_t type;
    uint16_t value;
    cc_constant(&type,&value);
    if(cc_family(type)!=cc_family(selector)) cc_fail(46);
    return value;
}
static void case_compare(uint16_t value) {
    cc_emit(0xd1); cc_emit(0xd5); /* retain the selector only while dispatching */
    cc_op16(0x21,value); cc_call(RT_COMPARE);
}
static void case_statement(SourcePosition start,uint16_t code) {
    uint8_t selector;
    uint16_t matches,exits=0,miss,skip,lo,hi;
    uint16_t before=cc.temporaries;
    cc_next(); selector=cc_expression();
    if(!cc_ordinal(selector)) cc_fail(41);
    cc_release(before);
    cc_keyword("of",15); cc_guard(); cc_emit(0xe5);
    if(cc_is("end")||cc_is("else")) cc_fail(41);
    cc_mapping(code,pc(),start,cc.token_start);
    while(!cc.error->code&&!cc_is("end")&&!cc_is("else")&&cc.token!=T_EOF) {
        start=cc.token_start; code=pc(); matches=0;
        do {
            lo=case_constant(selector);
            if(cc.token==T_RANGE) {
                cc_next(); hi=case_constant(selector);
                /* Two signed comparisons cover the entire ordinal domain
                 * without overflowing a computed high-low range width. */
                case_compare(lo); cc_emit(0xcb); cc_emit(0x7c);
                skip=cc_forward(0xc2); /* below low: try the next label */
                case_compare(hi); cc_emit(0x7d); cc_emit(0xfe); cc_emit(1);
                link_jump(0xc2,&matches); cc_patch(skip);
            } else {
                case_compare(lo); cc_emit(0x7c); cc_emit(0xb5);
                link_jump(0xca,&matches);
            }
            if(cc.token!=',') break;
            cc_next();
        } while(!cc.error->code);
        cc_expect(':',3); miss=cc_forward(0xc3); patch_jumps(matches);
        cc_emit(0xc1); /* match releases its selector before entering the arm */
        cc_mapping(code,pc(),start,cc.token_start);
        statement();
        code=pc(); link_jump(0xc3,&exits);
        cc_mapping(code,pc(),start,cc.token_start); cc_patch(miss);
        if(cc.token==';') cc_next(); else if(!cc_is("end")&&!cc_is("else")) cc_fail(1);
    }
    start=cc.token_start; code=pc(); cc_emit(0xc1); /* unmatched selector */
    cc_mapping(code,pc(),start,cc.token_start);
    if(cc_is("else")) {
        cc_next();
        while(!cc.error->code&&!cc_is("end")&&cc.token!=T_EOF) {
            statement();
            if(cc.token==';') cc_next(); else if(!cc_is("end")) cc_fail(1);
        }
    }
    cc_keyword("end",14); patch_jumps(exits);
}
static SourcePosition statement(void) {
    SourcePosition start=cc.token_start,head_end;
    uint16_t code=pc(),branch,other,loop,body;
    int16_t id;
    uint8_t newline,kind,target,indexed;
    uint16_t before=cc.temporaries;
    if(++cc.depth>24) { cc_fail(98); --cc.depth; return start; }
    id=cc.token==T_ID?cc_lookup():-1;
    if(cc.token==T_NUMBER||(id>=0&&cc.symbols[id].role==S_LABEL)) {
        id=label_lookup();
        if(id<0||cc.symbols[id].role!=S_LABEL) cc_fail(41);
        else label_define((uint16_t)id);
        if(cc.error->code) { --cc.depth; return start; }
        start=cc.token_start;
    }
    loop=cc.emitted;cc_user_interrupt();
    cc_mapping(code,pc(),start,cc.token_end);code=pc();
    if(cc_is("begin")) {
        cc_next();
        while(!cc.error->code&&!cc_is("end")&&cc.token!=T_EOF) {
            statement();
            if(cc.token==';') cc_next(); else if(!cc_is("end")) cc_fail(1);
        }
        start=cc.token_start;
        if(cc_is("end")) cc_next(); else cc_fail(14);
        --cc.depth; return start;
    } else if(cc_is("case")) {
        case_statement(start,code); --cc.depth; return start;
    } else if(cc_is("with")) {
        with_statement(start,code); --cc.depth; return start;
    } else if(cc_is("if")) {
        cc_next(); branch=condition(); cc_keyword("then",17);
        cc_mapping(code,pc(),start,cc.token_start);
        statement();
        if(cc_is("else")) {
            code=pc(); other=cc_forward(0xc3);
            cc_mapping(code,pc(),start,cc.token_start);
            cc_patch(branch); cc_next(); statement(); cc_patch(other);
        } else cc_patch(branch);
        --cc.depth; return start;
    } else if(cc_is("while")) {
        cc_next(); branch=condition(); cc_keyword("do",13);
        head_end=cc.token_start; cc_mapping(code,pc(),start,head_end);
        statement(); code=pc(); cc_address(0xc3,loop); cc_patch(branch);
        cc_mapping(code,pc(),start,head_end);
        --cc.depth; return start;
    } else if(cc_is("repeat")) {
        cc_next();
        while(!cc.error->code&&!cc_is("until")&&cc.token!=T_EOF) {
            statement();
            if(cc.token==';') cc_next(); else if(!cc_is("until")) cc_fail(1);
        }
        start=cc.token_start; code=pc(); cc_keyword("until",41);
        branch=condition();
        if(!cc.error->code) cc_word(branch,loop);
        cc_mapping(code,pc(),start,cc.token_start);
        --cc.depth; return start;
    } else if(cc_is("for")) {
        target=cc.user_interrupt;
        cc_next(); id=cc.token==T_ID?cc_lookup():-1;
        if(!cc_variable(id)) cc_fail(24);
        else if(cc.symbols[id].role==S_FIELD) cc_fail(41);
        else if(!cc_ordinal(cc.symbols[id].type)) cc_fail(30);
        else if(cc.symbols[id].control) cc_fail(44);
        cc_next(); cc_expect(T_ASSIGN,7);
        kind=cc_expression();
        if(cc_family(kind)==K_STRING) cc_fail(44);
        if(id>=0) cc_convert(cc.symbols[id].type,kind);
        cc_release(before);
        cc_guard(); cc_emit(0xe5); /* initial, not assigned until a nonzero trip */
        newline=cc_is("downto");
        if(!cc_is("to")&&!newline) cc_fail(18); else cc_next();
        kind=cc_expression();
        if(cc_family(kind)==K_STRING) cc_fail(44);
        if(id>=0) cc_convert(cc.symbols[id].type,kind);
        cc_release(before);
        cc_emit(0xd1); cc_guard(); cc_emit(0xe5); /* keep final bound across the body */
        cc_call(RT_COMPARE); /* DE initial survives; HL -> signed three-way */
        cc_emit(0x7d); cc_emit(0xfe); cc_emit(newline?255:1); branch=cc_forward(0xca);
        cc_emit(0xeb); if(id>=0) cc_store_symbol((uint16_t)id);
        cc_keyword("do",13); head_end=cc.token_start;
        cc_mapping(code,pc(),start,head_end);
        body=cc.emitted;
        /* The loop owns its interruption policy even if its body changes U.
         * The backward edge must visit a poll for an empty/disabled body. */
        code=pc();kind=cc.user_interrupt;cc.user_interrupt=target;
        cc_user_interrupt();cc.user_interrupt=kind;
        cc_mapping(code,pc(),start,head_end);
        if(id>=0) cc.symbols[id].control=1;
        ++cc.for_depth; statement(); --cc.for_depth;
        if(id>=0) cc.symbols[id].control=0;
        code=pc();
        if(id>=0) cc_load_symbol((uint16_t)id);
        cc_emit(0xd1); cc_guard(); cc_emit(0xd5); cc_emit(0xeb); cc_call(RT_COMPARE);
        cc_emit(0x7c); cc_emit(0xb5); other=cc_forward(0xca);
        if(id>=0) cc_load_symbol((uint16_t)id);
        cc_emit(newline?0x2b:0x23); if(id>=0) cc_store_symbol((uint16_t)id);
        cc_address(0xc3,body);
        cc_patch(branch); cc_patch(other); cc_emit(0xc1); /* exactly one owned bound word */
        cc_mapping(code,pc(),start,head_end);
        --cc.depth; return start;
    } else if(cc_is("goto")) {
        goto_statement();
    } else if(cc_is("inline")) {
        cc_inline();
    } else if((cc_is("write")||cc_is("writeln"))&&cc_lookup()<0) {
        cc_write(cc_is("writeln"));
    } else if(cc_is("str")&&cc_lookup()<0) {
        cc_str();
    } else if(cc_is("val")&&cc_lookup()<0) {
        cc_val();
    } else if((cc_is("read")||cc_is("readln"))&&cc_lookup()<0) {
        cc_read(cc_is("readln"));
    } else if((cc_is("delete")||cc_is("insert"))&&cc_lookup()<0) {
        cc_string_statement(cc_is("insert"));
    } else if(cc_is("randomize")&&cc_lookup()<0) {
        cc_next(); cc_randomize();
    } else if(cc_file_statement()) {
    } else if(cc_runtime_statement()) {
    } else if((cc_is("halt")||cc_is("exit"))&&cc_lookup()<0) {
        newline=cc_is("halt"); cc_next();
        if(newline) cc_finish(RT_HALT); else cc_exit();
    }
    else if(cc.token==T_ID&&!cc_is("end")) {
        id=cc_lookup();
        if(id<0) cc_fail(41);
        else if(cc.symbols[id].role==S_PROCEDURE) cc_invoke((uint16_t)id);
        else {
            if(!cc_variable(id)||cc.symbols[id].control) cc_fail(44);
            target=cc.symbols[id].type; cc_next();
            indexed=cc.symbols[id].role!=S_RESULT&&((cc.token=='['&&
                (cc_family(target)==K_STRING||cc_aggregate(target)))||
                (cc.token=='.'&&cc_family(target)==K_RECORD)||
                (cc.token=='^'&&cc_family(target)==K_POINTER));
            if(indexed) {
                cc_symbol_address((uint16_t)id); target=cc_index(target); cc_guard(); cc_emit(0xe5);
            }
            if(cc_family(target)==K_FILE) cc_fail(54);
            cc_expect(T_ASSIGN,7);
            kind=cc_aggregate(target)?cc_aggregate_actual(target):cc_expression();
            cc_convert(target,kind);
            if(indexed) cc_stack_operand(cc.temporaries-before);
            else if(cc_family(target)==K_STRING||cc_family(target)==K_REAL||cc_aggregate(target)||cc_family(target)==K_SET) cc_destination((uint16_t)id);
            if(cc_family(target)==K_STRING) {
                cc_emit(0x0e); cc_emit(cc.types[target].capacity);
                cc_call(cc_family(kind)==K_CHAR?RT_CHARCOPY:RT_COPY);
            } else if(cc_aggregate(target)||cc_family(target)==K_REAL) {
                cc_op16(0x01,cc.types[target].width); cc_call(RT_MOVE);
            } else if(cc_family(target)==K_SET) cc_set_store(target);
            else if(indexed) cc_store_indirect(target);
            else cc_store_symbol((uint16_t)id);
            if(indexed&&cc.temporaries!=before) cc_drop_stack(2);
            cc_release(before);
        }
    } else if(cc.token!=';'&&!cc_is("end")&&!cc_is("else")&&!cc_is("until")) cc_fail(54);
    cc_mapping(code,pc(),start,cc.token_start);
    --cc.depth;
    return start;
}
SourcePosition cc_statement(void) BACKEND { return statement(); }
/* Shared job bounds also drive the IDE options annotations. */
uint8_t compile_limits(uint16_t globals,uint16_t code,const CompileOptions *options,CompileLimits *limits) SOURCE_BLOCK_CALL {
    uint32_t start,end,top,needed;
    memset(limits,0,sizeof(*limits));
    if(globals>=PAGE_SIZE||(options&&options->kind>COMPILE_CHAIN))return 0;
    limits->minimum_start=GLOBAL_BASE+globals;
    start=options&&options->start?options->start:limits->minimum_start;
    end=options?options->end:0xfbffu;
    if(!code)code=1; /* UI platform bounds before a program has been compiled. */
    top=end+1;
    if(top>=2048UL+code)limits->maximum_start=(uint16_t)(((top-2048u)&~1UL)-code);
    needed=((start+code+1)&~1UL)+2048u-1;
    if(needed<0xcfffu)needed=0xcfffu;
    limits->minimum_end=needed>65535u?65535u:(uint16_t)needed;
    return end>=0xcfffu&&end<=0xfbffu&&start>=limits->minimum_start&&
        start<=limits->maximum_start&&end>=needed;
}
uint8_t compile(const Source *s,Build *b,Diagnostic *d) {
    return compile_project(s,0,b,d,0,0);
}
uint8_t compile_project(const Source *s,const Sources *buffers,Build *b,Diagnostic *d,uint8_t interruptible,const CompileOptions *options) {
    uint16_t i,j;
    SourcePosition begin;
    Source active;
    CompilerSources sources;
    memset(&cc,0,sizeof(cc));memset(&sources,0,sizeof(sources));
    sources.refs_page=255;sources.root=s;sources.options=options;
    cc.input=&active;cc.sources=&sources;cc.image=b;cc.error=d;
    cc.workspace=255;cc.stage_page=255;
    memset(d,0,sizeof(*d));memset(b,0,sizeof(*b));
    memset(b->pages,255,3);b->map_page=255;b->sources.page=255;
    cc.polling=interruptible;
    if(!cc_poll())goto failed;
    if(!cc_sources_prepare(s,buffers))goto failed;
    for(i=0;i<3;++i) {
        b->pages[i]=page_new(PAGE_IMAGE);
        if(b->pages[i]==255){d->code=98;goto failed;}
        for(j=0;j<PAGE_SIZE;++j) {
            if(!(j&255)&&!cc_poll())goto failed;
            far_set(b->pages[i],j,0);
        }
        page_initialized(b->pages[i],PAGE_SIZE);
    }
    b->map_page=page_new(PAGE_MAP);
    if(b->map_page==255){d->code=98;goto failed;}
    cc.workspace=page_new(PAGE_COMPILER);
    if(cc.workspace==255){d->code=98;goto failed;}
    for(i=0;i<96;++i)far_set(b->map_page,i,0);
    for(i=0;i<cc_runtime_size();++i) {
        if(!(i&255)&&!cc_poll())goto failed;
        cc_put(i,cc_runtime_get(i));
    }
    cc.absolute_mode=1;cc.var_strict=1;cc.index_speed=1;cc.with_limit=4;
    cc.input_fresh=1;cc.io_check=1;cc.console_control=1;cc.text_type=255;
    cc_new_type(K_INTEGER,2,-32768,32767,0);
    cc_new_type(K_STRING,0,0,0,0);
    cc_new_type(K_BOOLEAN,1,0,1,0);
    cc_new_type(K_CHAR,1,0,255,0);
    cc_new_type(K_INTEGER,1,0,255,0);
    cc_new_type(K_REAL,6,0,0,0);
    cc_system_init();
    cc.file_bytes=40+(uint16_t)strlen(s->path);
    cc_next();
    if(cc_is("program")) {
        cc_next();cc_expect(T_ID,53);
        if(cc.token=='('){cc_next();cc_expect(T_ID,53);while(cc.token==','){cc_next();cc_expect(T_ID,53);}cc_expect(')',5);}
        cc_expect(';',1);
    }
    b->code_origin=CC_CODE_BASE;cc.emitted=CC_CODE_BASE;
    if(options&&options->kind==COMPILE_CHAIN)cc_activation_begin();
    begin=cc_block();j=pc();cc_expect('.',10);
    if(cc.token!=T_EOF)cc_fail(58);
    cc_finish(RT_DONE);cc_mapping(j,pc(),begin,begin);
    cc_hooks_finish();cc_console_finish();cc_file_finish();cc_error_finish();
    if(d->code||!cc_place_code())goto failed;
    if(cc.stage_page!=255) {
        if(page_drop(cc.stage_page))cc.stage_page=255;
        else {d->code=98;goto failed;}
    }
    if(page_drop(cc.workspace))cc.workspace=255;
    else {d->code=98;goto failed;}
    if(page_drop(sources.refs_page))sources.refs_page=255;
    else {d->code=98;goto failed;}
    if(!cc_seal(s,options))goto failed;
    return 1;
failed:
    if(d->code!=COMPILE_INTERRUPTED)cc_sources_error();
    cc.polling=0;
    if(sources.refs_page!=255&&page_drop(sources.refs_page))sources.refs_page=255;
    if(cc.stage_page!=255&&page_drop(cc.stage_page))cc.stage_page=255;
    if(cc.workspace!=255&&page_drop(cc.workspace))cc.workspace=255;
    build_close(b);return 0;
}

/* Error provenance is captured while invocation-owned compiler inputs live. */
void cc_sources_error(void) ROUTINES {
    Source failed;const Source *original=0;int16_t id;
    cc.literal_length=0;
    if(cc.sources->external_error){strcpy(cc.name,cc.input->path);return;}
    if(!sources_get(&cc.image->sources,cc.sources->error_file,&failed)) {
        strcpy(cc.name,cc.sources->root->path);return;
    }
    strcpy(cc.name,failed.path);
    if(sources_hash(&cc.image->sources,cc.sources->error_file,cc.literal))cc.literal_length=32;
    if(!cc.sources->error_file)original=cc.sources->root;
    else if(cc.sources->buffers) {
        id=sources_find(cc.sources->buffers,failed.path);
        if(id>=0&&sources_get(cc.sources->buffers,(uint8_t)id,&failed))original=&failed;
    }
    if(original) {
        cc.error->start=source_editor_offset(original,cc.error->start);
        cc.error->end=source_editor_offset(original,cc.error->end);
    }
}
