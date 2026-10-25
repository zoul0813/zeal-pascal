#include "compiler.h"

static const char *const reserved[] = {
    "absolute","and","array","begin","case","const","div","do","downto","else","end",
    "external","file","for","forward","function","goto","if","in","inline","label","mod",
    "nil","not","of","or","overlay","packed","procedure","program","record","repeat","set",
    "shl","shr","string","then","to","type","until","var","while","with","xor"};

static uint8_t get(uint16_t p) { return p<cc.input->length ? far_get(cc.input->page,p) : 0; }
static uint8_t lower(uint8_t c) { return c>='A'&&c<='Z' ? c+32 : c; }
static uint8_t letter(uint8_t c) { c=lower(c); return (c>='a'&&c<='z')||c=='_'; }
static uint8_t digit(uint8_t c) { return c>='0'&&c<='9'; }
void cc_fail(uint8_t code) FRONTEND {
    if(!cc.error->code) { cc.error->code=code; cc.error->start=cc.token_start; cc.error->end=cc.token_end; }
}

void cc_put(uint16_t p,uint8_t v) FRONTEND { far_set(cc.image->pages[p/PAGE_SIZE],p%PAGE_SIZE,v); }
void cc_word(uint16_t p,uint16_t v) FRONTEND { cc_put(p,(uint8_t)v); cc_put(p+1,(uint8_t)(v>>8)); }



void cc_emit(uint8_t v) FRONTEND {
    if(cc.error->code||cc.suppress) return;
    /* Stage code in the third image page. cc_place_code bounds the completed
     * static/code image below C000, beneath the managed C800 stack floor. */
    if(cc.emitted>=0xc000u) { cc_fail(99); return; }
    cc_put(cc.emitted++,v);
}
static void emit16(uint16_t v) { cc_emit((uint8_t)v); cc_emit((uint8_t)(v>>8)); }
void cc_op16(uint8_t op,uint16_t value) FRONTEND { cc_emit(op); emit16(value); }
void cc_relocate(uint16_t operand) FRONTEND {
    uint16_t p=cc.relocations*2;
    if(cc.error->code||cc.suppress) return;
    if((uint32_t)p+2+(uint32_t)cc.routine_count*16>PAGE_SIZE) { cc_fail(99); return; }
    far_set(cc.workspace,p,(uint8_t)operand); far_set(cc.workspace,p+1,(uint8_t)(operand>>8));
    ++cc.relocations;
}
void cc_address(uint8_t op,uint16_t value) FRONTEND {
    cc_relocate(cc.emitted+1); cc_op16(op,value);
}
uint8_t cc_place_code(void) FRONTEND {
    uint16_t i,at,value,origin=GLOBAL_BASE+cc.image->globals_bytes;
    uint16_t count=cc.emitted-0x8000u,delta=origin-0xc000u;
    if((uint32_t)cc.image->globals_bytes+count>PAGE_SIZE) { cc_fail(99); return 0; }
    for(i=0;i<cc.relocations;++i) {
        at=far_get(cc.workspace,i*2)|((uint16_t)far_get(cc.workspace,i*2+1)<<8);
        if(at<0x8000u||at>=cc.emitted-1) { cc_fail(99); return 0; }
        value=far_get(cc.image->pages[2],at-0x8000u)|((uint16_t)far_get(cc.image->pages[2],at-0x8000u+1)<<8);
        cc_word(at,value+delta);
    }
    /* Temporary code occupies page 2. Its final extent is entirely below
     * page 2, after all declared globals/static locals, so copying cannot
     * overwrite bytes that have not yet been read. */
    for(i=0;i<count;++i) cc_put(origin-0x4000u+i,far_get(cc.image->pages[2],i));
    cc.image->code_origin=origin; cc.image->code_bytes=count;
    cc.image->length=origin-0x4000u+count;
    return 1;
}

uint16_t cc_forward(uint8_t op) FRONTEND { uint16_t at=cc.emitted+1; cc_address(op,0); return at; }
void cc_patch(uint16_t at) FRONTEND { if(!cc.error->code&&!cc.suppress) cc_word(at,0x4000u+cc.emitted); }
uint16_t cc_link(uint8_t opcode,uint16_t head) FRONTEND {
    if(cc.suppress) return head;
    uint16_t at=cc.emitted+1; cc_address(opcode,head); return at;
}
void cc_patch_chain(uint16_t head,uint16_t value) FRONTEND {
    uint16_t next;
    while(head&&!cc.error->code) {
        next=far_get(cc.image->pages[2],head-0x8000u)|((uint16_t)far_get(cc.image->pages[2],head-0x8000u+1)<<8);
        cc_word(head,value); head=next;
    }
}
void cc_reserve(uint16_t bytes) FRONTEND {
    /* Alternate registers are restored on both paths. The fixed 64-byte
     * reservation covers every nonrecursive runtime-helper path and syscall. */
    uint16_t pc=cc.emitted-(cc.image->code_origin-0x4000u);
    cc_emit(0xd9); cc_emit(0x08); cc_op16(0x21,(uint16_t)(0u-64u-bytes)); cc_emit(0x39); /* save AF too */
    cc_emit(0x30); cc_emit(5); /* wrap -> failure */
    cc_emit(0x7c); cc_emit(0xfe); cc_emit(0xc8); cc_emit(0x30); cc_emit(8);
    cc_emit(0x08); cc_emit(0xd9); cc_op16(0x21,pc); cc_op16(0xc3,RT_STACK);
    cc_op16(0x21,pc); cc_op16(0x22,RT_PC); cc_emit(0x08); cc_emit(0xd9);
}
void cc_guard(void) FRONTEND { cc_reserve(0); }
void cc_temporary(uint16_t bytes) FRONTEND {
    cc_reserve(bytes); cc_emit(0xeb); cc_op16(0x21,(uint16_t)(0u-bytes));
    cc_emit(0x39); cc_emit(0xf9); cc_emit(0xeb); /* preserve HL; DE = new SP */
    cc.temporaries+=bytes;
}
void cc_drop_stack(uint16_t bytes) FRONTEND {
    if(!bytes) return;
    cc_emit(0xeb); cc_op16(0x21,bytes); cc_emit(0x39); cc_emit(0xf9); cc_emit(0xeb);
}
void cc_release(uint16_t before) FRONTEND {
    cc_drop_stack(cc.temporaries-before); cc.temporaries=before;
}
void cc_stack_operand(uint16_t right_bytes) FRONTEND {
    if(!right_bytes) cc_emit(0xd1);
    else {
        cc_emit(0x44); cc_emit(0x4d); cc_op16(0x21,right_bytes); cc_emit(0x39);
        cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0x60); cc_emit(0x69);
    }
}
void cc_load_indirect(uint8_t type) FRONTEND {
    if(cc_family(type)==K_STRING) return;
    if(type==255) { cc_fail(44); return; }
    if(cc.types[type].width==1) { cc_emit(0x6e); cc_emit(0x26); cc_emit(0); }
    else { cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb); }
}
void cc_store_indirect(uint8_t type) FRONTEND {
    cc_emit(0x7d); cc_emit(0x12);
    if(cc.types[type].width==2) { cc_emit(0x13); cc_emit(0x7c); cc_emit(0x12); }
}
void cc_call(uint16_t address) FRONTEND { cc_guard(); cc_op16(0xcd,address); }
void cc_next(void) FRONTEND {
    uint8_t c,q,base,value;
    uint16_t n=0;
    uint32_t wide;
    if(cc.error->code) { cc.token=T_EOF; return; }
    for(;;) {
        c=get(cc.scan);
        if(cc.scan<cc.input->length && c<=32) { ++cc.scan; continue; }
        if(c=='{' || (c=='('&&get(cc.scan+1)=='*')) {
            q=c; cc.scan+=q=='{' ? 1 : 2;
            if(get(cc.scan)=='$') {
                cc.token_start=cc.scan; cc.token_end=cc.scan+1; ++cc.scan;
                do {
                    c=lower(get(cc.scan)); value=get(cc.scan+1)=='+';
                    if((c!='r'&&c!='a'&&c!='v')||(get(cc.scan+1)!='+'&&get(cc.scan+1)!='-')) {
                        cc_fail(93); cc.token=T_EOF; return;
                    }
                    if(c=='r') cc.range_check=value;
                    else if(c=='a') cc.absolute_mode=value;
                    else cc.var_strict=value;
                    cc.scan+=2;
                    if(get(cc.scan)!=',') break;
                    ++cc.scan;
                } while(!cc.error->code);
                while(get(cc.scan)==' '||get(cc.scan)=='\t') ++cc.scan;
                if(!(q=='{'?get(cc.scan)=='}':get(cc.scan)=='*'&&get(cc.scan+1)==')')) {
                    cc_fail(93); cc.token=T_EOF; return;
                }
            }
            while(cc.scan<cc.input->length && !(q=='{' ? get(cc.scan)=='}' : get(cc.scan)=='*'&&get(cc.scan+1)==')')) ++cc.scan;
            if(cc.scan==cc.input->length) { cc.token_start=cc.scan; cc.token_end=cc.scan; cc_fail(91); cc.token=T_EOF; return; }
            cc.scan+=q=='{' ? 1 : 2; continue;
        }
        break;
    }
    cc.token_start=cc.scan; cc.token_end=cc.scan+1; cc.hexadecimal=0; cc.literal_length=0;
    if(cc.scan==cc.input->length) { cc.token=T_EOF; cc.token_end=cc.scan; return; }
    if(letter(c)) {
        while(letter(get(cc.scan))||digit(get(cc.scan))) {
            if(n<127) cc.name[n++]=lower(get(cc.scan));
            ++cc.scan;
        }
        cc.name[n]=0; cc.token=T_ID;
        for(n=0;n<sizeof(reserved)/sizeof(reserved[0]);++n)
            if(!strcmp(cc.name,reserved[n])) { cc.token=T_RESERVED; break; }
    } else if(digit(c)||c=='$') {
        base=c=='$'?16:10; cc.hexadecimal=base==16;
        if(cc.hexadecimal) ++cc.scan;
        wide=0; n=0;
        while(digit(get(cc.scan))||(base==16&&lower(get(cc.scan))>='a'&&lower(get(cc.scan))<='f')) {
            c=lower(get(cc.scan++)); value=digit(c)?c-'0':c-'a'+10;
            wide=wide*base+value; ++n;
            if((base==16&&n>4)||wide>(base==16?65535UL:32768UL)) { cc_fail(56); break; }
        }
        if(!n) cc_fail(56);
        cc.magnitude=(uint16_t)wide; cc.token=T_NUMBER;
    } else if(c=='\''||c=='#'||c=='^') {
        do {
            c=get(cc.scan++);
            if(c=='\'') {
                for(;;) {
                    c=get(cc.scan++);
                    if(cc.scan>cc.input->length||c=='\n') { cc_fail(55); break; }
                    if(c=='\'') { if(get(cc.scan)!='\'') break; ++cc.scan; }
                    if(cc.literal_length==255) { cc_fail(55); break; }
                    cc.literal[cc.literal_length++]=c;
                }
            } else {
                if(c=='^') {
                    c=get(cc.scan++);
                    if(cc.scan>cc.input->length||c<'@'||c>'~') { cc_fail(55); break; }
                    value=c&31;
                } else {
                    base=get(cc.scan)=='$'?16:10; if(base==16) ++cc.scan;
                    n=0; wide=0;
                    while(digit(get(cc.scan))||(base==16&&lower(get(cc.scan))>='a'&&lower(get(cc.scan))<='f')) {
                        c=lower(get(cc.scan++)); wide=wide*base+(digit(c)?c-'0':c-'a'+10); ++n;
                        if((base==16&&n>4)||wide>255) { cc_fail(56); break; }
                    }
                    if(!n) cc_fail(56);
                    value=(uint8_t)wide;
                }
                if(cc.literal_length==255) { cc_fail(55); break; }
                cc.literal[cc.literal_length++]=value;
            }
            c=get(cc.scan);
        } while(!cc.error->code&&(c=='\''||c=='#'||c=='^'));
        cc.token=T_STRING;
    } else {
        ++cc.scan; cc.token=c;
        if(!strchr(";:.,()+-*/[]=<>",c)) cc_fail(58);
        if(c==':'&&get(cc.scan)=='=') { ++cc.scan; cc.token=T_ASSIGN; }
        if(c=='<'&&get(cc.scan)=='>') { ++cc.scan; cc.token=T_NE; }
        if(c=='<'&&get(cc.scan)=='=') { ++cc.scan; cc.token=T_LE; }
        if(c=='>'&&get(cc.scan)=='=') { ++cc.scan; cc.token=T_GE; }
        if(c=='.'&&get(cc.scan)=='.') { ++cc.scan; cc.token=T_RANGE; }
        if(c=='('&&get(cc.scan)=='.') { ++cc.scan; cc.token='['; }
        if(c=='.'&&get(cc.scan)==')') { ++cc.scan; cc.token=']'; }
    }
    cc.token_end=cc.scan;
}

void cc_expect(uint16_t t,uint8_t code) FRONTEND { if(cc.token!=t) cc_fail(code); else cc_next(); }

int16_t cc_lookup(void) FRONTEND {
    uint16_t i; uint8_t j,scope=cc.scope,n=(uint8_t)strlen(cc.name);
    for(;;) {
        i=cc.symbol_count;
        while(i--) if(cc.symbols[i].owner==scope&&cc.symbols[i].name_length==n) {
            for(j=0;j<n;++j) if(cc.name[j]!=lower(get(cc.symbols[i].name_start+j))) break;
            if(j==n) return (int16_t)i;
        }
        if(!scope) break;
        scope=(uint8_t)cc_rget(scope,R_PARENT);
    }
    return -1;
}
uint16_t cc_new_symbol(uint8_t role) FRONTEND {
    uint16_t id=cc.symbol_count;
    int16_t old=cc_lookup();
    if(cc.token!=T_ID) cc_fail(53);
    else if(old>=0&&cc.symbols[old].owner==cc.scope) cc_fail(43);
    else if(id==SYMBOL_LIMIT) cc_fail(98);
    if(cc.error->code) return 0;
    memset(&cc.symbols[id],0,sizeof(Symbol));
    cc.symbols[id].name_start=cc.token_start; cc.symbols[id].name_length=(uint8_t)strlen(cc.name);
    cc.symbols[id].role=role; cc.symbols[id].owner=cc.scope; ++cc.symbol_count;
    return id;
}
uint8_t cc_new_type(uint8_t family,uint16_t width,int16_t low,int16_t high,uint8_t capacity) FRONTEND {
    uint8_t id=cc.type_count;
    if(id==TYPE_LIMIT) { cc_fail(98); return 0; }
    cc.types[id].family=family; cc.types[id].width=width;
    cc.types[id].low=low; cc.types[id].high=high; cc.types[id].capacity=capacity;
    ++cc.type_count; return id;
}
uint8_t cc_family(uint8_t type) FRONTEND { return type<cc.type_count?cc.types[type].family:255; }
static uint8_t text_type(uint8_t type) { return cc_family(type)==K_CHAR||cc_family(type)==K_STRING; }
uint8_t cc_compatible(uint8_t left,uint8_t right) FRONTEND {
    return cc_family(left)==cc_family(right)||(text_type(left)&&text_type(right));
}
uint8_t cc_variable(int16_t id) FRONTEND {
    return id>=0&&(cc.symbols[id].role==S_VARIABLE||cc.symbols[id].role==S_TYPED||
           cc.symbols[id].role==S_VALUE||cc.symbols[id].role==S_REFERENCE||cc.symbols[id].role==S_RESULT);
}
uint16_t cc_rget(uint8_t scope,uint8_t field) FRONTEND {
    uint16_t at=PAGE_SIZE-(uint16_t)scope*16+field;
    uint16_t value=far_get(cc.workspace,at);
    if(field>R_FLAGS) value|=(uint16_t)far_get(cc.workspace,at+1)<<8;
    return value;
}
void cc_rput(uint8_t scope,uint8_t field,uint16_t value) FRONTEND {
    uint16_t at=PAGE_SIZE-(uint16_t)scope*16+field;
    far_set(cc.workspace,at,(uint8_t)value);
    if(field>R_FLAGS) far_set(cc.workspace,at+1,(uint8_t)(value>>8));
}
void cc_storage(uint16_t id,uint16_t width) FRONTEND {
    uint16_t count;
    if(cc.scope&&(cc_rget(cc.scope,R_FLAGS)&RF_RECURSIVE)) {
        count=cc_rget(cc.scope,R_LOCALS);
        if((uint32_t)count+width>0x7ff0u) { cc_fail(99); return; }
        count+=width; cc_rput(cc.scope,R_LOCALS,count);
        cc.symbols[id].address=(uint16_t)(0u-6u-count);
    } else {
        if((uint32_t)cc.image->globals_bytes+width>=PAGE_SIZE) { cc_fail(99); return; }
        cc.symbols[id].address=GLOBAL_BASE+cc.image->globals_bytes;
        cc.image->globals_bytes+=width;
    }
}
void cc_frame(uint8_t scope) FRONTEND {
    uint8_t current=cc.scope;
    if(!scope) { cc_op16(0x21,0); return; }
    cc_guard(); cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xe1); /* HL = IX */
    while(current!=scope&&!cc.error->code) {
        if(!current) { cc_fail(44); return; }
        cc_emit(0x2b); cc_emit(0x2b); cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
        current=(uint8_t)cc_rget(current,R_PARENT);
    }
}
void cc_slot(uint16_t id) FRONTEND {
    Symbol *s=&cc.symbols[id];
    if(s->role==S_TYPED) cc_address(0x21,s->address);
    else if(s->owner&&(cc_rget(s->owner,R_FLAGS)&RF_RECURSIVE)) {
        cc_frame(s->owner); cc_op16(0x11,s->address); cc_emit(0x19);
    } else cc_op16(0x21,s->address);
}
void cc_symbol_address(uint16_t id) FRONTEND {
    cc_slot(id);
    if(cc.symbols[id].role==S_REFERENCE) { cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb); }
}
void cc_destination(uint16_t id) FRONTEND {
    Symbol *s=&cc.symbols[id];
    if(s->role==S_REFERENCE||(s->owner&&(cc_rget(s->owner,R_FLAGS)&RF_RECURSIVE))) {
        cc_guard(); cc_emit(0xe5); cc_symbol_address(id); cc_emit(0xeb); cc_emit(0xe1);
    } else if(s->role==S_TYPED) cc_address(0x11,s->address);
    else cc_op16(0x11,cc.symbols[id].address);
}
void cc_load_symbol(uint16_t id) FRONTEND {
    Symbol *s=&cc.symbols[id];
    if(s->type==255) { cc_fail(44); return; }
    if(cc_family(s->type)==K_STRING) cc_symbol_address(id);
    else if(s->role==S_REFERENCE||(s->owner&&(cc_rget(s->owner,R_FLAGS)&RF_RECURSIVE))) {
        cc_symbol_address(id);
        cc_load_indirect(s->type);
    }
    else if(cc.types[s->type].width==1) {
        if(s->role==S_TYPED) cc_address(0x3a,s->address); else cc_op16(0x3a,s->address);
        cc_emit(0x6f); cc_emit(0x26); cc_emit(0);
    } else if(s->role==S_TYPED) cc_address(0x2a,s->address); else cc_op16(0x2a,s->address);
}
void cc_store_symbol(uint16_t id) FRONTEND {
    Symbol *s=&cc.symbols[id];
    uint8_t op;
    if(s->type==255) { cc_fail(44); return; }
    op=cc.types[s->type].width==1?0x32:0x22;
    if(s->role==S_REFERENCE||(s->owner&&(cc_rget(s->owner,R_FLAGS)&RF_RECURSIVE))) {
        cc_guard(); cc_emit(0xe5); cc_symbol_address(id); cc_emit(0xd1); cc_emit(0x73);
        if(op==0x22) { cc_emit(0x23); cc_emit(0x72); }
        return;
    }
    if(op==0x32) cc_emit(0x7d);
    if(cc.symbols[id].role==S_TYPED) cc_address(op,cc.symbols[id].address); else cc_op16(op,cc.symbols[id].address);
}
void cc_check_range(uint8_t type) FRONTEND {
    if(type==255) { cc_fail(44); return; }
    if(!cc.range_check||cc_family(type)==K_STRING) return;
    cc_op16(0x11,(uint16_t)cc.types[type].low); cc_op16(0x01,(uint16_t)cc.types[type].high); cc_call(RT_RANGE);
}
void cc_result_symbol(uint16_t routine) FRONTEND {
    uint16_t i; uint8_t j;
    Symbol *s=&cc.symbols[routine];
    for(i=0;i<cc.symbol_count;++i) if(cc.symbols[i].owner==cc.scope&&cc.symbols[i].name_length==s->name_length) {
        for(j=0;j<s->name_length;++j)
            if(lower(get(cc.symbols[i].name_start+j))!=lower(get(s->name_start+j))) break;
        if(j==s->name_length) { cc_fail(43); return; }
    }
    if(cc.symbol_count==SYMBOL_LIMIT) { cc_fail(98); return; }
    i=cc.symbol_count++; cc.symbols[i]=*s;
    cc.symbols[i].owner=cc.scope; cc.symbols[i].role=S_RESULT;
    cc_storage(i,cc.types[s->type].width);
}
void cc_convert(uint8_t target,uint8_t source) FRONTEND {
    if(target==255||source==255||!cc_compatible(target,source)) { cc_fail(44); return; }
    if(cc_family(target)==K_CHAR&&cc_family(source)==K_STRING) cc_call(RT_STRINGCHAR);
    cc_check_range(target);
}
/* String constants retain their source location, not a pointer into compiler
 * scratch memory. Re-decode only that literal while preserving parser state. */
void cc_constant_literal(uint16_t at) FRONTEND {
    uint16_t saved_scan=cc.scan,saved_token=cc.token,first=cc.token_start,last=cc.token_end;
    cc.scan=at; cc_next();
    cc.scan=saved_scan; cc.token=saved_token; cc.token_start=first; cc.token_end=last;
}
void cc_emit_literal(void) FRONTEND {
    uint16_t i,address=0x4000u+cc.emitted+3;
    cc_address(0xc3,address+cc.literal_length+1); cc_emit((uint8_t)cc.literal_length);
    for(i=0;i<cc.literal_length;++i) cc_emit(cc.literal[i]);
    cc_address(0x21,address);
}
int16_t cc_predefined_type(void) FRONTEND {
    return cc_is("integer")?K_INTEGER:cc_is("boolean")?K_BOOLEAN:cc_is("char")?K_CHAR:cc_is("byte")?K_BYTE:-1;
}
int16_t cc_named_type(void) FRONTEND {
    int16_t id=cc.token==T_ID?cc_lookup():-1;
    if(id>=0&&cc.symbols[id].role==S_TYPE) return cc.symbols[id].type;
    return id<0?cc_predefined_type():-1;
}
void cc_constant(uint8_t *type,uint16_t *value) FRONTEND {
    int16_t id;
    uint8_t sign=0;
    if(cc.token=='+'||cc.token=='-') { sign=cc.token=='-'?2:1; cc_next(); }
    *type=K_INTEGER; *value=0;
    if(cc.token==T_NUMBER) {
        if(!cc.hexadecimal&&cc.magnitude>32767u&&sign!=2) cc_fail(56);
        *value=cc.magnitude;
    } else if(cc.token==T_STRING&&!sign) {
        *type=cc.literal_length==1?K_CHAR:K_STRING;
        *value=cc.literal_length==1?cc.literal[0]:cc.token_start;
    } else if(cc.token==T_ID) {
        id=cc_lookup();
        if(id>=0&&cc.symbols[id].role==S_CONSTANT) { *type=cc.symbols[id].type; *value=cc.symbols[id].address; }
        else if(id<0&&(cc_is("true")||cc_is("false"))) { *type=K_BOOLEAN; *value=cc_is("true"); }
        else if(id<0&&cc_is("maxint")) *value=32767;
        else cc_fail(41);
    } else cc_fail(25);
    if(sign&&cc_family(*type)!=K_INTEGER) cc_fail(25);
    if(sign==2) *value=(uint16_t)(0u-*value);
    cc_next();
}
static uint8_t parse_type(void) {
    int16_t id;
    uint16_t lo,hi,count,member;
    uint8_t base,other,result;
    if(cc_is("string")) {
        cc_next(); cc_expect('[',8); cc_constant(&base,&lo);
        if(cc_family(base)!=K_INTEGER||lo<1||lo>255) cc_fail(49);
        cc_expect(']',9);
        return cc_new_type(K_STRING,lo+1,0,0,(uint8_t)lo);
    }
    if(cc.token=='(') {
        result=cc_new_type(cc.type_count,1,0,0,0); count=0; cc_next();
        do {
            member=cc_new_symbol(S_CONSTANT);
            cc.symbols[member].type=result; cc.symbols[member].address=count++;
            cc_next(); if(cc.token!=',') break; cc_next();
        } while(!cc.error->code);
        cc_expect(')',5);
        cc.types[result].high=(int16_t)(count-1); cc.types[result].width=count<=256?1:2;
        return result;
    }
    id=cc.token==T_ID?cc_lookup():-1;
    if(id>=0&&cc.symbols[id].role==S_TYPE) { result=cc.symbols[id].type; cc_next(); return result; }
    if(id<0&&(id=cc_predefined_type())>=0) { cc_next(); return (uint8_t)id; }
    cc_constant(&base,&lo);
    if(cc_family(base)==K_STRING) cc_fail(51);
    cc_expect(T_RANGE,41); cc_constant(&other,&hi);
    if(cc_family(base)!=cc_family(other)) cc_fail(51);
    if((int16_t)lo>(int16_t)hi) cc_fail(52);
    return cc_new_type(cc_family(base),cc_family(base)==K_INTEGER?(lo<=255&&hi<=255?1:2):cc.types[base].width,
                    (int16_t)lo,(int16_t)hi,0);
}
void cc_declarations(void) FRONTEND {
    uint16_t id,first,count,i,value;
    uint8_t section,type,initial;
    while((cc_is("var")||cc_is("const")||cc_is("type"))&&!cc.error->code) {
        section=cc_is("var")?S_VARIABLE:cc_is("const")?S_CONSTANT:S_TYPE; cc_next();
        while(cc.token==T_ID&&!cc.error->code) {
            first=cc.symbol_count;
            id=cc_new_symbol(section==S_VARIABLE?S_VARIABLE:S_PENDING); cc_next();
            if(section==S_VARIABLE) {
                while(cc.token==','&&!cc.error->code) { cc_next(); cc_new_symbol(S_VARIABLE); cc_next(); }
                count=cc.symbol_count-first; cc_expect(':',2); type=parse_type();
                for(i=first;i<first+count&&!cc.error->code;++i) {
                    cc.symbols[i].type=type; cc_storage(i,cc.types[type].width);
                }
            } else if(section==S_TYPE) {
                cc_expect('=',6); type=parse_type(); cc.symbols[id].type=type; cc.symbols[id].role=S_TYPE;
            } else {
                if(cc.token==':') {
                    cc_next(); type=parse_type(); cc_expect('=',6); cc_constant(&initial,&value);
                    if(!cc_compatible(type,initial)) cc_fail(44);
                    if(cc_family(type)==K_CHAR&&cc_family(initial)==K_STRING) {
                        cc_constant_literal(value);
                        if(cc.literal_length!=1) cc_fail(50);
                        value=cc.literal[0]; initial=K_CHAR;
                    }
                    if(cc_family(type)!=K_STRING&&((int16_t)value<cc.types[type].low||(int16_t)value>cc.types[type].high)) cc_fail(44);
                    cc.symbols[id].type=type; cc.symbols[id].initial_type=initial; cc.symbols[id].role=S_TYPED;
                } else {
                    cc_expect('=',6); cc_constant(&type,&value);
                    cc.symbols[id].type=type; cc.symbols[id].role=S_CONSTANT;
                }
                cc.symbols[id].address=value;
            }
            cc_expect(';',1);
        }
    }
}
void cc_typed_storage(uint16_t first) FRONTEND {
    uint16_t i,j,value,jump=0;
    for(i=first;i<cc.symbol_count&&!cc.error->code;++i) if(cc.symbols[i].role==S_TYPED) {
        if(!jump) jump=cc_forward(0xc3);
        value=cc.symbols[i].address; cc.symbols[i].address=0x4000u+cc.emitted;
        if(cc_family(cc.symbols[i].type)==K_STRING) {
            if(cc_family(cc.symbols[i].initial_type)==K_CHAR) { cc.literal[0]=(uint8_t)value; cc.literal_length=1; }
            else cc_constant_literal(value);
            if(cc.literal_length>cc.types[cc.symbols[i].type].capacity) cc.literal_length=cc.types[cc.symbols[i].type].capacity;
            cc_emit((uint8_t)cc.literal_length);
            for(j=0;j<cc.types[cc.symbols[i].type].capacity;++j) cc_emit(j<cc.literal_length?cc.literal[j]:0);
        } else {
            cc_emit((uint8_t)value); if(cc.types[cc.symbols[i].type].width==2) cc_emit((uint8_t)(value>>8));
        }
    }
    if(jump) cc_patch(jump);
}
