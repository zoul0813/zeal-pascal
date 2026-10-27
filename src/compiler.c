#ifdef __SDCC
#define compile compiler_compile
#define build_close compiler_build_close
#define build_check compiler_build_check
#define build_ready compiler_build_ready
#define build_locate compiler_build_locate
#define image_get compiler_image_get
#endif
#include "compiler.h"


uint8_t image_get(const Build *b,uint16_t p) { return far_get(b->pages[p/PAGE_SIZE],p%PAGE_SIZE); }

static void map8(uint16_t p,uint8_t v) { far_set(cc.image->map_page,p,v); }

static void map16(uint16_t p,uint16_t v) { map8(p,(uint8_t)v); map8(p+1,(uint8_t)(v>>8)); }

static void map32(uint16_t p,uint32_t v) { uint8_t i; for(i=0;i<4;++i) map8(p+i,(uint8_t)(v>>(i*8))); }

static uint16_t pc(void) { return cc.emitted-CC_CODE_BASE; }

static void mapping(uint16_t begin,uint16_t end,uint16_t first,uint16_t last) {
    uint16_t offset=96+cc.file_bytes+cc.entries*16;
    if(begin==end||cc.error->code) return;
    if(offset>PAGE_SIZE-16) { cc_fail(99); return; }
    map16(offset,begin); map16(offset+2,end); map16(offset+4,0); map16(offset+6,0);
    map32(offset+8,first); map32(offset+12,last); ++cc.entries;
}
void cc_mapping(uint16_t begin,uint16_t end,uint16_t first,uint16_t last) BACKEND { mapping(begin,end,first,last); }
static uint16_t condition(void) {
    if(cc_family(cc_expression())!=K_BOOLEAN) cc_fail(20);
    cc_emit(0x7c); cc_emit(0xb5); /* A = H | L; false has Z set */
    return cc_forward(0xca);
}
static uint16_t statement(void);
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
static void with_statement(uint16_t start,uint16_t code) {
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
    cc_keyword("do",13); mapping(code,pc(),start,cc.token_start);
    statement(); code=pc();
    while(cc.with_depth>saved) {
        cc_emit(0xe1); cc_emit(0xfd); cc_emit(0xe1); --cc.with_depth;
    }
    mapping(code,pc(),start,cc.token_start);
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
static void case_statement(uint16_t start,uint16_t code) {
    uint8_t selector;
    uint16_t matches,exits=0,miss,skip,lo,hi;
    uint16_t before=cc.temporaries;
    cc_next(); selector=cc_expression();
    if(!cc_ordinal(selector)) cc_fail(41);
    cc_release(before);
    cc_keyword("of",15); cc_guard(); cc_emit(0xe5);
    if(cc_is("end")||cc_is("else")) cc_fail(41);
    mapping(code,pc(),start,cc.token_start);
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
        mapping(code,pc(),start,cc.token_start);
        statement();
        code=pc(); link_jump(0xc3,&exits);
        mapping(code,pc(),start,cc.token_start); cc_patch(miss);
        if(cc.token==';') cc_next(); else if(!cc_is("end")&&!cc_is("else")) cc_fail(1);
    }
    start=cc.token_start; code=pc(); cc_emit(0xc1); /* unmatched selector */
    mapping(code,pc(),start,cc.token_start);
    if(cc_is("else")) {
        cc_next();
        while(!cc.error->code&&!cc_is("end")&&cc.token!=T_EOF) {
            statement();
            if(cc.token==';') cc_next(); else if(!cc_is("end")) cc_fail(1);
        }
    }
    cc_keyword("end",14); patch_jumps(exits);
}
static uint16_t statement(void) {
    uint16_t start=cc.token_start, code=pc(),branch,other,loop,body,head_end;
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
        mapping(code,pc(),start,cc.token_start);
        statement();
        if(cc_is("else")) {
            code=pc(); other=cc_forward(0xc3);
            mapping(code,pc(),start,cc.token_start);
            cc_patch(branch); cc_next(); statement(); cc_patch(other);
        } else cc_patch(branch);
        --cc.depth; return start;
    } else if(cc_is("while")) {
        loop=cc.emitted; cc_next(); branch=condition(); cc_keyword("do",13);
        head_end=cc.token_start; mapping(code,pc(),start,head_end);
        statement(); code=pc(); cc_address(0xc3,loop); cc_patch(branch);
        mapping(code,pc(),start,head_end);
        --cc.depth; return start;
    } else if(cc_is("repeat")) {
        loop=cc.emitted; cc_next();
        while(!cc.error->code&&!cc_is("until")&&cc.token!=T_EOF) {
            statement();
            if(cc.token==';') cc_next(); else if(!cc_is("until")) cc_fail(1);
        }
        start=cc.token_start; code=pc(); cc_keyword("until",41);
        branch=condition();
        if(!cc.error->code) cc_word(branch,loop);
        mapping(code,pc(),start,cc.token_start);
        --cc.depth; return start;
    } else if(cc_is("for")) {
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
        mapping(code,pc(),start,head_end);
        body=cc.emitted;
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
        mapping(code,pc(),start,head_end);
        --cc.depth; return start;
    } else if(cc_is("goto")) {
        goto_statement();
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
    mapping(code,pc(),start,cc.token_start);
    --cc.depth;
    return start;
}
uint16_t cc_statement(void) BACKEND { return statement(); }
void build_close(Build *b) {
    uint8_t i;
    b->valid=0;
    for(i=0;i<3;++i) if(b->pages[i]!=255&&page_drop(b->pages[i])) b->pages[i]=255;
    if(b->map_page!=255&&page_drop(b->map_page)) b->map_page=255;
}
static void hash_image(const Build *b,uint8_t out[32]) {
    Sha256 hash; uint16_t i;
    sha_init(&hash);
    for(i=0;i<b->length;++i) sha_byte(&hash,i>=36&&i<68?0:image_get(b,i));
    sha_end(&hash,out);
}
static void hash_map(const Build *b,uint8_t out[32]) {
    Sha256 hash; uint16_t i;
    sha_init(&hash);
    for(i=0;i<b->map_length;++i) sha_byte(&hash,i>=20&&i<52?0:far_get(b->map_page,i));
    sha_end(&hash,out);
}
uint8_t compile(const Source *s,Build *b,Diagnostic *d) {
    uint16_t i,j;
    uint8_t digest[32];
    Sha256 hash;
    cc.image=b; cc.error=d;
    cc.workspace=255; cc.stage_page=255;
    memset(d,0,sizeof(*d));
    memset(b,0,sizeof(*b)); memset(b->pages,255,3); b->map_page=255;
    if(!source_snapshot(s,&cc.snapshot)) { d->code=98; return 0; }
    cc.input=&cc.snapshot;
    for(i=0;i<3;++i) {
        b->pages[i]=page_new(PAGE_IMAGE);
        if(b->pages[i]==255) { d->code=98; goto failed; }
        for(j=0;j<PAGE_SIZE;++j) far_set(b->pages[i],j,0);
        page_initialized(b->pages[i],PAGE_SIZE);
    }
    b->map_page=page_new(PAGE_MAP);
    if(b->map_page==255) { d->code=98; goto failed; }
    cc.workspace=page_new(PAGE_COMPILER);
    if(cc.workspace==255) { d->code=98; goto failed; }
    for(i=0;i<96;++i) map8(i,0);
    for(i=0;i<cc_runtime_size();++i) cc_put(i,cc_runtime_get(i));
    cc.scan=0; cc.symbol_count=0; cc.depth=0; cc.entries=0; cc.range_check=0; cc.type_count=0; cc.relocations=0;
    cc.scope=0; cc.routine_count=0; cc.absolute_mode=1; cc.var_strict=1; cc.nesting=0; cc.suppress=0; cc.with_depth=0; cc.with_limit=4;
    cc.temporaries=0; cc.for_depth=0; cc.input_fresh=1; cc.io_check=1;
    cc.random_entry=0; cc.random_state=0;
    cc.text_type=255;
    cc.parameter_entry=0;
    cc.file_entry=0;
    cc.standard_table_operand=0;
    cc_new_type(K_INTEGER,2,-32768,32767,0);
    cc_new_type(K_STRING,0,0,0,0); /* expression/literal string, not a storage type */
    cc_new_type(K_BOOLEAN,1,0,1,0);
    cc_new_type(K_CHAR,1,0,255,0);
    cc_new_type(K_INTEGER,1,0,255,0); /* Byte shares the Integer family */
    cc_new_type(K_REAL,6,0,0,0);
    cc_system_init();
    cc.file_bytes=40+(uint16_t)strlen(s->path);
    cc_next();
    if(cc_is("program")) {
        cc_next(); cc_expect(T_ID,53);
        if(cc.token=='(') { cc_next(); cc_expect(T_ID,53); while(cc.token==',') { cc_next(); cc_expect(T_ID,53); } cc_expect(')',5); }
        cc_expect(';',1);
    }
    b->code_origin=CC_CODE_BASE;
    cc.emitted=CC_CODE_BASE;
    i=cc_block();
    j=pc();
    cc_expect('.',10);
    if(cc.token!=T_EOF) cc_fail(58);
    cc_finish(RT_DONE);
    mapping(j,pc(),i,i);
    cc_file_finish();
    if(d->code) goto failed;
    if(!cc_place_code()) goto failed;
    if(cc.stage_page!=255) {
        if(page_drop(cc.stage_page)) cc.stage_page=255;
        else { d->code=98; goto failed; }
    }
    if(page_drop(cc.workspace)) cc.workspace=255;
    else { d->code=98; goto failed; }
    b->revision=s->revision;
    source_hash(s,b->source_hash);
    /* ZMAP: one canonical resident source, deterministic half-open mappings. */
    for(i=0;i<4;++i) map8(i,"ZMAP"[i]);
    map8(4,1); map16(6,96); map16(8,1); map16(10,b->code_origin); map16(12,b->code_bytes);
    map16(14,1); map32(16,cc.entries); map32(84,cc.file_bytes); map32(88,(uint32_t)cc.entries*16);
    map16(96,0); map16(98,(uint16_t)strlen(s->path)); map32(100,cc.snapshot.length);
    for(i=0;i<32;++i) map8(104+i,b->source_hash[i]);
    for(i=0;i<strlen(s->path);++i) map8(136+i,(uint8_t)s->path[i]);
    sha_init(&hash);
    for(i=96;i<96+cc.file_bytes;++i) sha_byte(&hash,far_get(b->map_page,i));
    sha_end(&hash,digest);
    for(i=0;i<32;++i) { cc_put(68+i,digest[i]); map8(52+i,digest[i]); }
    b->map_length=96+cc.file_bytes+cc.entries*16;
    page_initialized(b->map_page,b->map_length);
    hash_map(b,digest);
    for(i=0;i<32;++i) cc_put(100+i,digest[i]);
    cc_word(10,b->length); cc_word(20,b->code_origin); cc_word(22,b->code_bytes);
    cc_word(24,GLOBAL_BASE); cc_word(26,b->globals_bytes); cc_word(28,0x4000u+b->length);
    hash_image(b,b->id);
    for(i=0;i<32;++i) { cc_put(36+i,b->id[i]); map8(20+i,b->id[i]); }
    for(i=0;i<3;++i) b->versions[i]=page_versions[b->pages[i]];
    b->versions[3]=page_versions[b->map_page]; b->versions[4]=page_versions[s->page];
    b->valid=1;
    source_close(&cc.snapshot);
    return 1;
failed:
    if(cc.stage_page!=255&&page_drop(cc.stage_page)) cc.stage_page=255;
    if(cc.workspace!=255&&page_drop(cc.workspace)) cc.workspace=255;
    d->start=source_editor_offset(s,d->start);
    d->end=source_editor_offset(s,d->end);
    source_close(&cc.snapshot);
    build_close(b);
    return 0;
}
uint8_t build_check(const Source *s,const Build *b) {
    uint8_t hash[32], expected[32], i;
    if(!b->valid||b->revision!=s->revision) return 0;
    source_hash(s,hash); if(memcmp(hash,b->source_hash,32)) return 0;
    hash_image(b,hash); if(memcmp(hash,b->id,32)) return 0;
    hash_map(b,hash);
    for(i=0;i<32;++i) expected[i]=image_get(b,100+i);
    return !memcmp(hash,expected,32);
}
uint8_t build_ready(const Source *s,const Build *b) {
    uint8_t i;
    if(!b->valid||b->revision!=s->revision) return 0;
    /* Compiler-sealed pages are private and immutable. RAM execution copies
     * them. The single far-store core detects every subsequent modification;
     * changed versions require the full cryptographic check again. */
    for(i=0;i<3;++i) if(b->versions[i]!=page_versions[b->pages[i]]) return build_check(s,b);
    if(b->versions[3]!=page_versions[b->map_page]||b->versions[4]!=page_versions[s->page]) return build_check(s,b);
    return 1;
}
static uint16_t read16(uint8_t p,uint16_t offset) { return far_get(p,offset)|((uint16_t)far_get(p,offset+1)<<8); }
uint8_t build_locate(const Source *s,const Build *b,uint16_t pc,uint16_t *position) {
    uint16_t i, start, end, n;
    if(!build_ready(s,b)||pc>=b->code_bytes) return 0;
    n=read16(b->map_page,84);
    for(i=96+n;i+16<=b->map_length;i+=16) {
        start=read16(b->map_page,i); end=read16(b->map_page,i+2);
        if(pc>=start&&pc<end) {
            start=read16(b->map_page,i+8);
            if(start>s->length||read16(b->map_page,i+10)!=0) return 0;
            *position=source_editor_offset(s,start); return 1;
        }
    }
    return 0;
}
