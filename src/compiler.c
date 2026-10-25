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

static uint16_t pc(void) { return cc.emitted-(cc.image->code_origin-0x4000u); }

static uint8_t expression(void);
enum { F_ORD,F_CHR,F_SUCC,F_PRED,F_ABS,F_SQR,F_ODD,F_HI,F_LO,F_SWAP,F_UPCASE,F_SIZEOF,F_ADDR };
static int16_t predefined_function(void) {
    static const char *const functions[]={"ord","chr","succ","pred","abs","sqr","odd","hi","lo","swap","upcase","sizeof","addr"};
    uint8_t i;
    for(i=0;i<sizeof(functions)/sizeof(functions[0]);++i) if(cc_is(functions[i])) return i;
    return -1;
}
static void negate(void) { cc_emit(0x7d); cc_emit(0x2f); cc_emit(0x6f); cc_emit(0x7c); cc_emit(0x2f); cc_emit(0x67); cc_emit(0x23); }
static uint8_t intrinsic(uint8_t op) {
    uint8_t kind=K_INTEGER;
    int16_t id;
    cc_next(); cc_expect('(',4);
    if(op==F_SIZEOF||op==F_ADDR) {
        id=cc.token==T_ID?cc_lookup():-1;
        if(op==F_ADDR) {
            if(id>=0&&(cc.symbols[id].role==S_PROCEDURE||cc.symbols[id].role==S_FUNCTION||cc.symbols[id].role==S_RESULT)) cc_routine_address((uint16_t)id);
            else if(!cc_variable(id)) cc_fail(44); else cc_symbol_address((uint16_t)id);
        } else if(id>=0&&(cc_variable(id)||cc.symbols[id].role==S_TYPE)&&cc.symbols[id].type!=255) cc_op16(0x21,cc.types[cc.symbols[id].type].width);
        else if(id<0&&(id=cc_predefined_type())>=0) cc_op16(0x21,cc.types[id].width);
        else cc_fail(41);
        cc_next(); cc_expect(')',5); return kind;
    }
    kind=expression(); cc_expect(')',5);
    if(cc.error->code) return K_INTEGER;
    if(op==F_ORD||op==F_SUCC||op==F_PRED) {
        if(cc_family(kind)==K_STRING) cc_fail(44);
        if(op==F_ORD) return K_INTEGER;
        cc_emit(op==F_SUCC?0x23:0x2b); cc_check_range(kind);
        if(cc.types[kind].width==1) { cc_emit(0x26); cc_emit(0); }
        return kind;
    }
    if(op==F_UPCASE) {
        if(cc_family(kind)==K_STRING) cc_call(RT_STRINGCHAR); else if(cc_family(kind)!=K_CHAR) cc_fail(44);
        cc_emit(0x7d); cc_emit(0xfe); cc_emit('a'); cc_emit(0x38); cc_emit(7);
        cc_emit(0xfe); cc_emit('z'+1); cc_emit(0x30); cc_emit(3); cc_emit(0xd6); cc_emit(32); cc_emit(0x6f);
        return K_CHAR;
    }
    if(cc_family(kind)!=K_INTEGER) cc_fail(23);
    if(op==F_CHR) { cc_check_range(K_CHAR); cc_emit(0x26); cc_emit(0); return K_CHAR; }
    if(op==F_ABS) { cc_emit(0xcb); cc_emit(0x7c); cc_emit(0x28); cc_emit(7); negate(); }
    else if(op==F_SQR) { cc_emit(0x54); cc_emit(0x5d); cc_call(RT_MUL); }
    else if(op==F_ODD) { cc_emit(0x7d); cc_emit(0xe6); cc_emit(1); cc_emit(0x6f); cc_emit(0x26); cc_emit(0); return K_BOOLEAN; }
    else if(op==F_HI) { cc_emit(0x6c); cc_emit(0x26); cc_emit(0); }
    else if(op==F_LO) { cc_emit(0x26); cc_emit(0); }
    else if(op==F_SWAP) { cc_emit(0x7c); cc_emit(0x65); cc_emit(0x6f); }
    return K_INTEGER;
}
static uint8_t factor(void) {
    int16_t id,fn=-1;
    uint8_t negative,kind=K_INTEGER;
    if(++cc.depth>24) { cc_fail(98); --cc.depth; return 0; }
    if(cc.token=='+'||cc.token=='-') {
        negative=cc.token=='-'; cc_next();
        if(negative&&cc.token==T_NUMBER&&!cc.hexadecimal&&cc.magnitude==32768u) {
            cc_op16(0x21,0x8000); cc_next();
        } else {
            if(cc_family(factor())!=K_INTEGER) cc_fail(47);
            if(negative) negate();
        }
    } else if(cc_is("not")) {
        cc_next(); kind=factor();
        if(cc_family(kind)==K_BOOLEAN) { cc_emit(0x7d); cc_emit(0xee); cc_emit(1); cc_emit(0x6f); kind=K_BOOLEAN; }
        else if(cc_family(kind)==K_INTEGER) { cc_emit(0x7d); cc_emit(0x2f); cc_emit(0x6f); cc_emit(0x7c); cc_emit(0x2f); cc_emit(0x67); kind=K_INTEGER; }
        else cc_fail(47);
    } else if(cc.token==T_NUMBER) {
        if(!cc.hexadecimal&&cc.magnitude>32767u) cc_fail(56);
        cc_op16(0x21,cc.magnitude); cc_next();
    } else if(cc.token==T_STRING) {
        if(cc.literal_length==1) { cc_op16(0x21,cc.literal[0]); kind=K_CHAR; }
        else { cc_emit_literal(); kind=K_STRING; }
        cc_next();
    } else if(cc.token=='(') { cc_next(); kind=expression(); cc_expect(')',5); }
    else if(cc.token==T_ID) {
        id=cc_lookup();
        if((id>=0&&cc.symbols[id].role==S_TYPE)||(id<0&&(fn=cc_predefined_type())>=0)) {
            kind=id>=0?cc.symbols[id].type:(uint8_t)fn;
            cc_next(); cc_expect('(',4);
            negative=expression(); cc_expect(')',5);
            if(cc_family(kind)==K_STRING) cc_fail(44);
            else if(cc_family(kind)==K_CHAR&&cc_family(negative)==K_STRING) cc_call(RT_STRINGCHAR);
            else if(cc_family(negative)==K_STRING) cc_fail(44);
            cc_check_range(kind);
            if(cc.types[kind].width==1) { cc_emit(0x26); cc_emit(0); }
        } else if(id<0&&(fn=predefined_function())>=0) kind=intrinsic((uint8_t)fn);
        else if(id<0&&(cc_is("true")||cc_is("false"))) {
            cc_op16(0x21,cc_is("true")?1:0); kind=K_BOOLEAN; cc_next();
        } else if(id<0&&cc_is("maxint")) { cc_op16(0x21,32767); cc_next();
        } else if(id<0) cc_fail(41);
        else if(cc.symbols[id].role==S_CONSTANT) {
            kind=cc.symbols[id].type;
            if(cc_family(kind)==K_STRING) { cc_constant_literal(cc.symbols[id].address); cc_emit_literal(); }
            else cc_op16(0x21,cc.symbols[id].address);
            cc_next();
        } else if(cc.symbols[id].role==S_FUNCTION||cc.symbols[id].role==S_RESULT) kind=cc_invoke((uint16_t)id);
        else if(cc_variable(id)) {
            kind=cc.symbols[id].type; cc_load_symbol((uint16_t)id); cc_next();
        } else cc_fail(41);
    } else cc_fail(54);
    --cc.depth;
    return kind;
}
static void bitwise(uint8_t op) {
    /* DE left, HL right. Boolean operands are already normalized to 0/1. */
    cc_emit(0x7d); cc_emit(op==0?0xa3:op==1?0xb3:0xab); cc_emit(0x6f);
    cc_emit(0x7c); cc_emit(op==0?0xa2:op==1?0xb2:0xaa); cc_emit(0x67);
}
static uint8_t term(void) {
    uint8_t op,right,base,kind=factor();
    while(!cc.error->code&&(cc.token=='*'||cc_is("div")||cc_is("mod")||cc_is("and")||cc_is("shl")||cc_is("shr"))) {
        op=cc.token=='*'?0:cc_is("div")?1:cc_is("mod")?2:cc_is("and")?3:cc_is("shl")?4:5;
        base=cc_family(kind);
        if(base!=K_INTEGER&&(op!=3||base!=K_BOOLEAN)) cc_fail(47);
        cc_next(); cc_guard(); cc_emit(0xe5); right=factor(); cc_emit(0xd1);
        if(cc_family(right)!=base) cc_fail(47);
        if(op==3) bitwise(0);
        else cc_call(op==0?RT_MUL:op==1?RT_DIV:op==2?RT_MOD:op==4?RT_SHL:RT_SHR);
        kind=base==K_INTEGER?K_INTEGER:K_BOOLEAN;
    }
    return kind;
}
static uint8_t simple_expression(void) {
    uint16_t op;
    uint8_t right,base,kind=term();
    while(!cc.error->code&&(cc.token=='+'||cc.token=='-'||cc_is("or")||cc_is("xor"))) {
        op=cc.token=='+'||cc.token=='-'?cc.token:cc_is("or")?'|':'^';
        base=cc_family(kind);
        if(base!=K_INTEGER&&((op=='+'||op=='-')||base!=K_BOOLEAN)) cc_fail(47);
        cc_next(); cc_guard(); cc_emit(0xe5); right=term(); cc_emit(0xd1);
        if(cc_family(right)!=base) cc_fail(47);
        if(op=='+') cc_emit(0x19);
        else if(op=='-') { cc_emit(0xeb); cc_emit(0xb7); cc_emit(0xed); cc_emit(0x52); }
        else bitwise(op=='|'?1:2);
        kind=base==K_INTEGER?K_INTEGER:K_BOOLEAN;
    }
    return kind;
}
static uint8_t expression(void) {
    uint8_t kind=simple_expression(),right,jump;
    uint16_t op=cc.token;
    if(op!='='&&op!=T_NE&&op!='<'&&op!='>'&&op!=T_LE&&op!=T_GE) return kind;
    cc_next(); cc_guard(); cc_emit(0xe5); right=simple_expression(); cc_emit(0xd1);
    if(!cc_compatible(kind,right)) cc_fail(47);
    if(cc_family(kind)==K_STRING||cc_family(right)==K_STRING) {
        cc_emit(0x3e); cc_emit((cc_family(kind)==K_CHAR?1:0)|(cc_family(right)==K_CHAR?2:0)); cc_call(RT_TEXTCMP);
    } else cc_call(RT_COMPARE);
    /* Three-way comparison returns exactly -1, 0, +1. Normalize every
     * relational expression to a one-byte Boolean, held zero-extended in HL. */
    if(op=='='||op==T_NE) { cc_emit(0x7c); cc_emit(0xb5); jump=op=='='?0x28:0x20; }
    else if(op=='<'||op==T_GE) { cc_emit(0xcb); cc_emit(0x7c); jump=op=='<'?0x20:0x28; }
    else { cc_emit(0x7d); cc_emit(0xfe); cc_emit(1); jump=op=='>'?0x28:0x20; }
    cc_op16(0x11,1); cc_emit(jump); cc_emit(3); cc_op16(0x11,0); cc_emit(0xeb);
    return K_BOOLEAN;
}
uint8_t cc_expression(void) BACKEND { return expression(); }
static void mapping(uint16_t begin,uint16_t end,uint16_t first,uint16_t last) {
    uint16_t offset=96+cc.file_bytes+cc.entries*16;
    if(begin==end||cc.error->code) return;
    if(offset>PAGE_SIZE-16) { cc_fail(99); return; }
    map16(offset,begin); map16(offset+2,end); map16(offset+4,0); map16(offset+6,0);
    map32(offset+8,first); map32(offset+12,last); ++cc.entries;
}
void cc_mapping(uint16_t begin,uint16_t end,uint16_t first,uint16_t last) BACKEND { mapping(begin,end,first,last); }
static uint16_t condition(void) {
    if(cc_family(expression())!=K_BOOLEAN) cc_fail(20);
    cc_emit(0x7c); cc_emit(0xb5); /* A = H | L; false has Z set */
    return cc_forward(0xca);
}
static void statement(void);
/* Forward operands double as a private patch list until the image is sealed.
 * Each nested Case owns its own heads; no extra compiler workspace is needed. */
static void link_jump(uint8_t opcode,uint16_t *head) {
    *head=cc_link(opcode,*head);
}
static void patch_jumps(uint16_t head) {
    cc_patch_chain(head,0x4000u+cc.emitted);
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
    cc_next(); selector=expression();
    if(cc_family(selector)==K_STRING) cc_fail(41);
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
static void statement(void) {
    uint16_t start=cc.token_start, code=pc(),branch,other,loop,body,head_end;
    int16_t id;
    uint8_t newline,kind;
    if(++cc.depth>24) { cc_fail(98); --cc.depth; return; }
    if(cc_is("begin")) {
        cc_next();
        while(!cc.error->code&&!cc_is("end")&&cc.token!=T_EOF) {
            statement();
            if(cc.token==';') cc_next(); else if(!cc_is("end")) cc_fail(1);
        }
        if(cc_is("end")) cc_next(); else cc_fail(14);
        --cc.depth; return;
    } else if(cc_is("case")) {
        case_statement(start,code); --cc.depth; return;
    } else if(cc_is("if")) {
        cc_next(); branch=condition(); cc_keyword("then",17);
        mapping(code,pc(),start,cc.token_start);
        statement();
        if(cc_is("else")) {
            code=pc(); other=cc_forward(0xc3);
            mapping(code,pc(),start,cc.token_start);
            cc_patch(branch); cc_next(); statement(); cc_patch(other);
        } else cc_patch(branch);
        --cc.depth; return;
    } else if(cc_is("while")) {
        loop=cc.emitted; cc_next(); branch=condition(); cc_keyword("do",13);
        head_end=cc.token_start; mapping(code,pc(),start,head_end);
        statement(); code=pc(); cc_address(0xc3,0x4000u+loop); cc_patch(branch);
        mapping(code,pc(),start,head_end);
        --cc.depth; return;
    } else if(cc_is("repeat")) {
        loop=cc.emitted; cc_next();
        while(!cc.error->code&&!cc_is("until")&&cc.token!=T_EOF) {
            statement();
            if(cc.token==';') cc_next(); else if(!cc_is("until")) cc_fail(1);
        }
        start=cc.token_start; code=pc(); cc_keyword("until",41);
        branch=condition();
        if(!cc.error->code) cc_word(branch,0x4000u+loop);
        mapping(code,pc(),start,cc.token_start);
        --cc.depth; return;
    } else if(cc_is("for")) {
        cc_next(); id=cc.token==T_ID?cc_lookup():-1;
        if(!cc_variable(id)) cc_fail(24);
        else if(cc_family(cc.symbols[id].type)==K_STRING||cc.symbols[id].control) cc_fail(44);
        cc_next(); cc_expect(T_ASSIGN,7);
        kind=expression();
        if(id>=0) cc_convert(cc.symbols[id].type,kind);
        cc_guard(); cc_emit(0xe5); /* initial, not assigned until a nonzero trip */
        newline=cc_is("downto");
        if(!cc_is("to")&&!newline) cc_fail(18); else cc_next();
        kind=expression();
        if(id>=0) cc_convert(cc.symbols[id].type,kind);
        cc_emit(0xd1); cc_guard(); cc_emit(0xe5); /* keep final bound across the body */
        cc_call(RT_COMPARE); /* DE initial survives; HL -> signed three-way */
        cc_emit(0x7d); cc_emit(0xfe); cc_emit(newline?255:1); branch=cc_forward(0xca);
        cc_emit(0xeb); if(id>=0) cc_store_symbol((uint16_t)id);
        cc_keyword("do",13); head_end=cc.token_start;
        mapping(code,pc(),start,head_end);
        body=cc.emitted;
        if(id>=0) cc.symbols[id].control=1;
        statement();
        if(id>=0) cc.symbols[id].control=0;
        code=pc();
        if(id>=0) cc_load_symbol((uint16_t)id);
        cc_emit(0xd1); cc_guard(); cc_emit(0xd5); cc_emit(0xeb); cc_call(RT_COMPARE);
        cc_emit(0x7c); cc_emit(0xb5); other=cc_forward(0xca);
        if(id>=0) cc_load_symbol((uint16_t)id);
        cc_emit(newline?0x2b:0x23); if(id>=0) cc_store_symbol((uint16_t)id);
        cc_address(0xc3,0x4000u+body);
        cc_patch(branch); cc_patch(other); cc_emit(0xc1); /* exactly one owned bound word */
        mapping(code,pc(),start,head_end);
        --cc.depth; return;
    } else if((cc_is("write")||cc_is("writeln"))&&cc_lookup()<0) {
        newline=cc_is("writeln"); cc_next();
        if(cc.token=='(') {
            cc_next();
            do {
                kind=cc_family(expression());
                if(kind>K_CHAR) cc_fail(44);
                if(kind==K_CHAR) cc_emit(0x7d);
                cc_call(kind==K_STRING?RT_STRING:kind==K_BOOLEAN?RT_BOOLEAN:kind==K_CHAR?RT_CHAR:RT_INTEGER);
                if(cc.token!=',') break;
                cc_next();
            } while(!cc.error->code);
            cc_expect(')',5);
        } else if(!newline) cc_fail(4);
        if(newline) {
            cc_emit(0x3e); cc_emit(13); cc_call(RT_CHAR);
            cc_emit(0x3e); cc_emit(10); cc_call(RT_CHAR);
        }
    } else if(cc_is("readln")&&cc_lookup()<0) {
        cc_next(); cc_call(RT_READLINE);
        if(cc.token=='(') {
            cc_next();
            do {
                id=cc.token==T_ID?cc_lookup():-1;
                if(id<0) { cc_fail(41); break; }
                if(!cc_variable(id)||cc_family(cc.symbols[id].type)!=K_STRING||cc.symbols[id].control) { cc_fail(44); break; }
                cc_destination((uint16_t)id);
                cc_emit(0x0e); cc_emit(cc.types[cc.symbols[id].type].capacity); cc_call(RT_READSTRING);
                cc_next(); if(cc.token!=',') break; cc_next();
            } while(!cc.error->code);
            cc_expect(')',5);
        }
    } else if((cc_is("halt")||cc_is("exit"))&&cc_lookup()<0) {
        newline=cc_is("halt"); cc_next();
        if(newline) cc_op16(0xc3,RT_HALT); else cc_exit();
    }
    else if(cc.token==T_ID&&!cc_is("end")) {
        id=cc_lookup();
        if(id<0) cc_fail(41);
        else if(cc.symbols[id].role==S_PROCEDURE) cc_invoke((uint16_t)id);
        else {
            if(!cc_variable(id)||cc.symbols[id].control) cc_fail(44);
            cc_next(); cc_expect(T_ASSIGN,7); kind=expression();
            cc_convert(cc.symbols[id].type,kind);
            if(cc_family(cc.symbols[id].type)==K_STRING) {
                cc_destination((uint16_t)id); cc_emit(0x0e); cc_emit(cc.types[cc.symbols[id].type].capacity);
                cc_call(cc_family(kind)==K_CHAR?RT_CHARCOPY:RT_COPY);
            } else cc_store_symbol((uint16_t)id);
        }
    } else if(cc.token!=';'&&!cc_is("end")&&!cc_is("else")&&!cc_is("until")) cc_fail(54);
    mapping(code,pc(),start,cc.token_start);
    --cc.depth;
}
void cc_statement(void) BACKEND { statement(); }
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
    cc.workspace=255;
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
    for(i=0;i<(uint16_t)(runtime_end-runtime_start);++i) cc_put(i,runtime_start[i]);
    cc.scan=0; cc.symbol_count=0; cc.depth=0; cc.entries=0; cc.range_check=0; cc.type_count=0; cc.relocations=0;
    cc.scope=0; cc.routine_count=0; cc.absolute_mode=1; cc.var_strict=1; cc.nesting=0;
    cc_new_type(K_INTEGER,2,-32768,32767,0);
    cc_new_type(K_STRING,0,0,0,0); /* expression/literal string, not a storage type */
    cc_new_type(K_BOOLEAN,1,0,1,0);
    cc_new_type(K_CHAR,1,0,255,0);
    cc_new_type(K_INTEGER,1,0,255,0); /* Byte shares the Integer family */
    cc.file_bytes=40+(uint16_t)strlen(s->path);
    cc_next();
    if(cc_is("program")) {
        cc_next(); cc_expect(T_ID,53);
        if(cc.token=='(') { cc_next(); cc_expect(T_ID,53); while(cc.token==',') { cc_next(); cc_expect(T_ID,53); } cc_expect(')',5); }
        cc_expect(';',1);
    }
    b->code_origin=0xc000u;
    cc.emitted=0x8000u;
    cc_block();
    cc_expect('.',10);
    if(cc.token!=T_EOF) cc_fail(58);
    cc_op16(0xc3,RT_DONE);
    if(d->code) goto failed;
    if(!cc_place_code()) goto failed;
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
