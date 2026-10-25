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
    if(cc.error->code) return;
    /* Initial runtime has no heap; keep all static/code bytes below C000 so
     * its checked C800 stack floor also dominates static storage + headroom. */
    if(cc.emitted>=0x8000u) { cc_fail(99); return; }
    cc_put(cc.emitted++,v);
}
static void emit16(uint16_t v) { cc_emit((uint8_t)v); cc_emit((uint8_t)(v>>8)); }
void cc_op16(uint8_t op,uint16_t value) FRONTEND { cc_emit(op); emit16(value); }

uint16_t cc_forward(uint8_t op) FRONTEND { uint16_t at=cc.emitted+1; cc_op16(op,0); return at; }
void cc_patch(uint16_t at) FRONTEND { if(!cc.error->code) cc_word(at,0x4000u+cc.emitted); }
void cc_guard(void) FRONTEND {
    /* Alternate registers are restored on both paths. The fixed 64-byte
     * reservation covers every nonrecursive runtime-helper path and syscall. */
    uint16_t pc=cc.emitted-(cc.image->code_origin-0x4000u);
    cc_emit(0xd9); cc_emit(0x08); cc_op16(0x21,0xffc0); cc_emit(0x39); /* save AF too */
    cc_emit(0x30); cc_emit(5); /* wrap -> failure */
    cc_emit(0x7c); cc_emit(0xfe); cc_emit(0xc8); cc_emit(0x30); cc_emit(8);
    cc_emit(0x08); cc_emit(0xd9); cc_op16(0x21,pc); cc_op16(0xc3,RT_STACK);
    cc_op16(0x21,pc); cc_op16(0x22,RT_PC); cc_emit(0x08); cc_emit(0xd9);
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
                if(lower(get(cc.scan))!='r'||(get(cc.scan+1)!='+'&&get(cc.scan+1)!='-')) {
                    cc_fail(93); cc.token=T_EOF; return;
                }
                cc.range_check=get(cc.scan+1)=='+'; cc.scan+=2;
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
    uint16_t i; uint8_t j,n=(uint8_t)strlen(cc.name);
    for(i=0;i<cc.symbol_count;++i) if(cc.symbols[i].name_length==n) {
        for(j=0;j<n;++j) if(cc.name[j]!=lower(get(cc.symbols[i].name_start+j))) break;
        if(j==n) return (int16_t)i;
    }
    return -1;
}
static uint16_t add_symbol(uint8_t role) {
    uint16_t id=cc.symbol_count;
    if(cc.token!=T_ID) cc_fail(53);
    else if(cc_lookup()>=0) cc_fail(43);
    else if(id==SYMBOL_LIMIT) cc_fail(98);
    if(cc.error->code) return 0;
    memset(&cc.symbols[id],0,sizeof(Symbol));
    cc.symbols[id].name_start=cc.token_start; cc.symbols[id].name_length=(uint8_t)strlen(cc.name);
    cc.symbols[id].role=role; ++cc.symbol_count;
    return id;
}
uint8_t cc_new_type(uint8_t family,uint16_t width,int16_t low,int16_t high,uint8_t capacity) FRONTEND {
    uint8_t id=cc.type_count;
    if(id==TYPE_LIMIT) { cc_fail(98); return 0; }
    cc.types[id].family=family; cc.types[id].width=width;
    cc.types[id].low=low; cc.types[id].high=high; cc.types[id].capacity=capacity;
    ++cc.type_count; return id;
}
uint8_t cc_family(uint8_t type) FRONTEND { return cc.types[type].family; }
static uint8_t text_type(uint8_t type) { return cc_family(type)==K_CHAR||cc_family(type)==K_STRING; }
uint8_t cc_compatible(uint8_t left,uint8_t right) FRONTEND {
    return cc_family(left)==cc_family(right)||(text_type(left)&&text_type(right));
}
uint8_t cc_variable(int16_t id) FRONTEND {
    return id>=0&&(cc.symbols[id].role==S_VARIABLE||cc.symbols[id].role==S_TYPED);
}
void cc_load_symbol(uint16_t id) FRONTEND {
    Symbol *s=&cc.symbols[id];
    if(cc_family(s->type)==K_STRING) cc_op16(0x21,s->address);
    else if(cc.types[s->type].width==1) { cc_op16(0x3a,s->address); cc_emit(0x6f); cc_emit(0x26); cc_emit(0); }
    else cc_op16(0x2a,s->address);
}
void cc_store_symbol(uint16_t id) FRONTEND {
    if(cc.types[cc.symbols[id].type].width==1) { cc_emit(0x7d); cc_op16(0x32,cc.symbols[id].address); }
    else cc_op16(0x22,cc.symbols[id].address);
}
void cc_check_range(uint8_t type) FRONTEND {
    if(!cc.range_check||cc_family(type)==K_STRING) return;
    cc_op16(0x11,(uint16_t)cc.types[type].low); cc_op16(0x01,(uint16_t)cc.types[type].high); cc_call(RT_RANGE);
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
    cc_op16(0xc3,address+cc.literal_length+1); cc_emit((uint8_t)cc.literal_length);
    for(i=0;i<cc.literal_length;++i) cc_emit(cc.literal[i]);
    cc_op16(0x21,address);
}
int16_t cc_predefined_type(void) FRONTEND {
    return cc_is("integer")?K_INTEGER:cc_is("boolean")?K_BOOLEAN:cc_is("char")?K_CHAR:cc_is("byte")?K_BYTE:-1;
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
            member=add_symbol(S_CONSTANT);
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
            id=add_symbol(section==S_VARIABLE?S_VARIABLE:S_PENDING); cc_next();
            if(section==S_VARIABLE) {
                while(cc.token==','&&!cc.error->code) { cc_next(); add_symbol(S_VARIABLE); cc_next(); }
                count=cc.symbol_count-first; cc_expect(':',2); type=parse_type();
                for(i=first;i<first+count&&!cc.error->code;++i) {
                    if(cc.image->globals_bytes+cc.types[type].width>=0x4000u) { cc_fail(99); break; }
                    cc.symbols[i].type=type; cc.symbols[i].address=GLOBAL_BASE+cc.image->globals_bytes;
                    cc.image->globals_bytes+=cc.types[type].width;
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
void cc_typed_storage(void) FRONTEND {
    uint16_t i,j,value,jump=0;
    for(i=0;i<cc.symbol_count&&!cc.error->code;++i) if(cc.symbols[i].role==S_TYPED) {
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
