#ifdef __SDCC
#define compile compiler_compile
#define build_close compiler_build_close
#define build_check compiler_build_check
#define build_ready compiler_build_ready
#define build_locate compiler_build_locate
#define image_get compiler_image_get
#endif
#include "zp.h"

/* One synchronous compiler owns this workspace. No source mutation or second
 * compile can run until it returns. Capacities fail with original 98/99. */
enum { T_EOF=256,T_ID,T_NUMBER,T_STRING,T_ASSIGN,T_RESERVED,T_NE,T_LE,T_GE };
enum { K_INTEGER,K_STRING,K_BOOLEAN };
typedef struct { char name[128]; uint16_t address; uint8_t kind,capacity,control; } Symbol;
static Symbol symbols[32];
static uint8_t symbol_count, depth;
static const Source *input;
static Source snapshot;
static Build *image;
static Diagnostic *error;
static uint16_t scan, token, token_start, token_end, magnitude, literal_length;
static uint8_t literal[256], hexadecimal;
static char name[128];
static const char *const reserved[] = {
    "absolute","and","array","begin","case","const","div","do","downto","else","end",
    "external","file","for","forward","function","goto","if","in","inline","label","mod",
    "nil","not","of","or","overlay","packed","procedure","program","record","repeat","set",
    "shl","shr","string","then","to","type","until","var","while","with","xor"};
static uint16_t emitted, entries, file_bytes;

static uint8_t get(uint16_t p) { return p<input->length ? far_get(input->page,p) : 0; }
static uint8_t lower(uint8_t c) { return c>='A'&&c<='Z' ? c+32 : c; }
static uint8_t letter(uint8_t c) { c=lower(c); return (c>='a'&&c<='z')||c=='_'; }
static uint8_t digit(uint8_t c) { return c>='0'&&c<='9'; }
static void fail(uint8_t code) {
    if(!error->code) { error->code=code; error->start=token_start; error->end=token_end; }
}
uint8_t image_get(const Build *b,uint16_t p) { return far_get(b->pages[p/PAGE_SIZE],p%PAGE_SIZE); }
static void put(uint16_t p,uint8_t v) { far_set(image->pages[p/PAGE_SIZE],p%PAGE_SIZE,v); }
static void word(uint16_t p,uint16_t v) { put(p,(uint8_t)v); put(p+1,(uint8_t)(v>>8)); }
static void map8(uint16_t p,uint8_t v) { far_set(image->map_page,p,v); }
static void map16(uint16_t p,uint16_t v) { map8(p,(uint8_t)v); map8(p+1,(uint8_t)(v>>8)); }
static void map32(uint16_t p,uint32_t v) { uint8_t i; for(i=0;i<4;++i) map8(p+i,(uint8_t)(v>>(i*8))); }
static void emit(uint8_t v) {
    if(error->code) return;
    /* Initial runtime has no heap; keep all static/code bytes below C000 so
     * its checked C800 stack floor also dominates static storage + headroom. */
    if(emitted>=0x8000u) { fail(99); return; }
    put(emitted++,v);
}
static void emit16(uint16_t v) { emit((uint8_t)v); emit((uint8_t)(v>>8)); }
static void op16(uint8_t op,uint16_t value) { emit(op); emit16(value); }
static uint16_t pc(void) { return emitted-(image->code_origin-0x4000u); }
static uint16_t forward(uint8_t op) { uint16_t at=emitted+1; op16(op,0); return at; }
static void patch(uint16_t at) { if(!error->code) word(at,0x4000u+emitted); }
static void guard(void) {
    /* Alternate registers are restored on both paths. The fixed 64-byte
     * reservation covers every nonrecursive runtime-helper path and syscall. */
    uint16_t pc=emitted-(image->code_origin-0x4000u);
    emit(0xd9); emit(0x08); op16(0x21,0xffc0); emit(0x39); /* save AF too */
    emit(0x30); emit(5); /* wrap -> failure */
    emit(0x7c); emit(0xfe); emit(0xc8); emit(0x30); emit(8);
    emit(0x08); emit(0xd9); op16(0x21,pc); op16(0xc3,RT_STACK);
    op16(0x21,pc); op16(0x22,RT_PC); emit(0x08); emit(0xd9);
}
static void call(uint16_t address) { guard(); op16(0xcd,address); }
static void next(void) {
    uint8_t c,q,base,value;
    uint16_t n=0;
    uint32_t wide;
    if(error->code) { token=T_EOF; return; }
    for(;;) {
        c=get(scan);
        if(scan<input->length && c<=32) { ++scan; continue; }
        if(c=='{' || (c=='('&&get(scan+1)=='*')) {
            q=c; scan+=q=='{' ? 1 : 2;
            if(get(scan)=='$') { token_start=scan; token_end=scan+1; fail(93); token=T_EOF; return; }
            while(scan<input->length && !(q=='{' ? get(scan)=='}' : get(scan)=='*'&&get(scan+1)==')')) ++scan;
            if(scan==input->length) { token_start=scan; token_end=scan; fail(91); token=T_EOF; return; }
            scan+=q=='{' ? 1 : 2; continue;
        }
        break;
    }
    token_start=scan; token_end=scan+1; hexadecimal=0; literal_length=0;
    if(scan==input->length) { token=T_EOF; token_end=scan; return; }
    if(letter(c)) {
        while(letter(get(scan))||digit(get(scan))) {
            if(n<127) name[n++]=lower(get(scan));
            ++scan;
        }
        name[n]=0; token=T_ID;
        for(n=0;n<sizeof(reserved)/sizeof(reserved[0]);++n)
            if(!strcmp(name,reserved[n])) { token=T_RESERVED; break; }
    } else if(digit(c)||c=='$') {
        base=c=='$'?16:10; hexadecimal=base==16;
        if(hexadecimal) ++scan;
        wide=0; n=0;
        while(digit(get(scan))||(base==16&&lower(get(scan))>='a'&&lower(get(scan))<='f')) {
            c=lower(get(scan++)); value=digit(c)?c-'0':c-'a'+10;
            wide=wide*base+value; ++n;
            if(wide>(base==16?65535UL:32768UL)) { fail(56); break; }
        }
        if(!n) fail(56);
        magnitude=(uint16_t)wide; token=T_NUMBER;
    } else if(c=='\'') {
        ++scan;
        for(;;) {
            c=get(scan++);
            if(scan>input->length||c=='\n') { fail(55); break; }
            if(c=='\'') { if(get(scan)!='\'') break; ++scan; }
            if(literal_length==255) { fail(55); break; }
            literal[literal_length++]=c;
        }
        token=T_STRING;
    } else {
        ++scan; token=c;
        if(!strchr(";:.,()+-*/[]=<>",c)) fail(58);
        if(c==':'&&get(scan)=='=') { ++scan; token=T_ASSIGN; }
        if(c=='<'&&get(scan)=='>') { ++scan; token=T_NE; }
        if(c=='<'&&get(scan)=='=') { ++scan; token=T_LE; }
        if(c=='>'&&get(scan)=='=') { ++scan; token=T_GE; }
    }
    token_end=scan;
}
static uint8_t is(const char *s) { return (token==T_ID||token==T_RESERVED)&&!strcmp(name,s); }
static void expect(uint16_t t,uint8_t code) { if(token!=t) fail(code); else next(); }
static void keyword(const char *s,uint8_t code) { if(!is(s)) fail(code); else next(); }
static int8_t lookup(void) {
    uint8_t i;
    for(i=0;i<symbol_count;++i) if(!strcmp(name,symbols[i].name)) return (int8_t)i;
    return -1;
}
static void load_symbol(uint8_t id) {
    Symbol *s=&symbols[id];
    if(s->kind==K_STRING) op16(0x21,s->address);
    else if(s->kind==K_BOOLEAN) { op16(0x3a,s->address); emit(0x6f); emit(0x26); emit(0); }
    else op16(0x2a,s->address);
}
static void store_symbol(uint8_t id) {
    if(symbols[id].kind==K_BOOLEAN) { emit(0x7d); op16(0x32,symbols[id].address); }
    else op16(0x22,symbols[id].address);
}
static uint8_t expression(void);
static uint8_t factor(void) {
    int8_t id;
    uint8_t negative,kind=K_INTEGER;
    uint16_t i,address;
    if(++depth>24) { fail(98); --depth; return 0; }
    if(token=='+'||token=='-') {
        negative=token=='-'; next();
        if(negative&&token==T_NUMBER&&!hexadecimal&&magnitude==32768u) {
            op16(0x21,0x8000); next();
        } else {
            if(factor()!=K_INTEGER) fail(47);
            if(negative) { emit(0x7d); emit(0x2f); emit(0x6f); emit(0x7c); emit(0x2f); emit(0x67); emit(0x23); }
        }
    } else if(is("not")) {
        next(); kind=factor();
        if(kind==K_BOOLEAN) { emit(0x7d); emit(0xee); emit(1); emit(0x6f); }
        else if(kind==K_INTEGER) { emit(0x7d); emit(0x2f); emit(0x6f); emit(0x7c); emit(0x2f); emit(0x67); }
        else fail(47);
    } else if(token==T_NUMBER) {
        if(!hexadecimal&&magnitude>32767u) fail(56);
        op16(0x21,magnitude); next();
    } else if(token==T_STRING) {
        /* A literal lives in the immutable code image, skipped at execution. */
        address=0x4000u+emitted+3;
        op16(0xc3,address+literal_length+1);
        emit((uint8_t)literal_length);
        for(i=0;i<literal_length;++i) emit(literal[i]);
        op16(0x21,address); kind=K_STRING; next();
    } else if(token=='(') { next(); kind=expression(); expect(')',5); }
    else if(token==T_ID) {
        id=lookup();
        if(id<0&&(is("true")||is("false"))) {
            op16(0x21,is("true")?1:0); kind=K_BOOLEAN; next();
        } else if(id<0) fail(41);
        else {
            kind=symbols[id].kind; load_symbol((uint8_t)id); next();
        }
    } else fail(54);
    --depth;
    return kind;
}
static void bitwise(uint8_t op) {
    /* DE left, HL right. Boolean operands are already normalized to 0/1. */
    emit(0x7d); emit(op==0?0xa3:op==1?0xb3:0xab); emit(0x6f);
    emit(0x7c); emit(op==0?0xa2:op==1?0xb2:0xaa); emit(0x67);
}
static uint8_t term(void) {
    uint8_t op,right,kind=factor();
    while(!error->code&&(token=='*'||is("div")||is("mod")||is("and")||is("shl")||is("shr"))) {
        op=token=='*'?0:is("div")?1:is("mod")?2:is("and")?3:is("shl")?4:5;
        if(kind==K_STRING||(op!=3&&kind!=K_INTEGER)) fail(47);
        next(); guard(); emit(0xe5); right=factor(); emit(0xd1);
        if(right!=kind) fail(47);
        if(op==3) bitwise(0);
        else call(op==0?RT_MUL:op==1?RT_DIV:op==2?RT_MOD:op==4?RT_SHL:RT_SHR);
    }
    return kind;
}
static uint8_t simple_expression(void) {
    uint16_t op;
    uint8_t right,kind=term();
    while(!error->code&&(token=='+'||token=='-'||is("or")||is("xor"))) {
        op=token=='+'||token=='-'?token:is("or")?'|':'^';
        if(kind==K_STRING||((op=='+'||op=='-')&&kind!=K_INTEGER)) fail(47);
        next(); guard(); emit(0xe5); right=term(); emit(0xd1);
        if(right!=kind) fail(47);
        if(op=='+') emit(0x19);
        else if(op=='-') { emit(0xeb); emit(0xb7); emit(0xed); emit(0x52); }
        else bitwise(op=='|'?1:2);
    }
    return kind;
}
static uint8_t expression(void) {
    uint8_t kind=simple_expression(),right,jump;
    uint16_t op=token;
    if(op!='='&&op!=T_NE&&op!='<'&&op!='>'&&op!=T_LE&&op!=T_GE) return kind;
    next(); guard(); emit(0xe5); right=simple_expression(); emit(0xd1);
    if(kind!=right) fail(47);
    call(kind==K_STRING?RT_STRCMP:RT_COMPARE);
    /* Three-way comparison returns exactly -1, 0, +1. Normalize every
     * relational expression to a one-byte Boolean, held zero-extended in HL. */
    if(op=='='||op==T_NE) { emit(0x7c); emit(0xb5); jump=op=='='?0x28:0x20; }
    else if(op=='<'||op==T_GE) { emit(0xcb); emit(0x7c); jump=op=='<'?0x20:0x28; }
    else { emit(0x7d); emit(0xfe); emit(1); jump=op=='>'?0x28:0x20; }
    op16(0x11,1); emit(jump); emit(3); op16(0x11,0); emit(0xeb);
    return K_BOOLEAN;
}
static void mapping(uint16_t begin,uint16_t end,uint16_t first,uint16_t last) {
    uint16_t offset=96+file_bytes+entries*16;
    if(begin==end||error->code) return;
    if(offset>PAGE_SIZE-16) { fail(99); return; }
    map16(offset,begin); map16(offset+2,end); map16(offset+4,0); map16(offset+6,0);
    map32(offset+8,first); map32(offset+12,last); ++entries;
}
static uint16_t condition(void) {
    if(expression()!=K_BOOLEAN) fail(20);
    emit(0x7c); emit(0xb5); /* A = H | L; false has Z set */
    return forward(0xca);
}
static void statement(void) {
    uint16_t start=token_start, code=pc(),branch,other,loop,body,head_end;
    int8_t id;
    uint8_t newline,kind;
    if(++depth>24) { fail(98); --depth; return; }
    if(is("begin")) {
        next();
        while(!error->code&&!is("end")&&token!=T_EOF) {
            statement();
            if(token==';') next(); else if(!is("end")) fail(1);
        }
        if(is("end")) next(); else fail(14);
        --depth; return;
    } else if(is("if")) {
        next(); branch=condition(); keyword("then",17);
        mapping(code,pc(),start,token_start);
        statement();
        if(is("else")) {
            code=pc(); other=forward(0xc3);
            mapping(code,pc(),start,token_start);
            patch(branch); next(); statement(); patch(other);
        } else patch(branch);
        --depth; return;
    } else if(is("while")) {
        loop=emitted; next(); branch=condition(); keyword("do",13);
        head_end=token_start; mapping(code,pc(),start,head_end);
        statement(); code=pc(); op16(0xc3,0x4000u+loop); patch(branch);
        mapping(code,pc(),start,head_end);
        --depth; return;
    } else if(is("repeat")) {
        loop=emitted; next();
        while(!error->code&&!is("until")&&token!=T_EOF) {
            statement();
            if(token==';') next(); else if(!is("until")) fail(1);
        }
        start=token_start; code=pc(); keyword("until",41);
        branch=condition();
        if(!error->code) word(branch,0x4000u+loop);
        mapping(code,pc(),start,token_start);
        --depth; return;
    } else if(is("for")) {
        next(); id=token==T_ID?lookup():-1;
        if(id<0) fail(24);
        else if(symbols[id].kind==K_STRING||symbols[id].control) fail(44);
        next(); expect(T_ASSIGN,7);
        kind=expression();
        if(id>=0&&kind!=symbols[id].kind) fail(44);
        guard(); emit(0xe5); /* initial, not assigned until a nonzero trip */
        newline=is("downto");
        if(!is("to")&&!newline) fail(18); else next();
        kind=expression();
        if(id>=0&&kind!=symbols[id].kind) fail(44);
        emit(0xd1); guard(); emit(0xe5); /* keep final bound across the body */
        call(RT_COMPARE); /* DE initial survives; HL -> signed three-way */
        emit(0x7d); emit(0xfe); emit(newline?255:1); branch=forward(0xca);
        emit(0xeb); if(id>=0) store_symbol((uint8_t)id);
        keyword("do",13); head_end=token_start;
        mapping(code,pc(),start,head_end);
        body=emitted;
        if(id>=0) symbols[id].control=1;
        statement();
        if(id>=0) symbols[id].control=0;
        code=pc();
        if(id>=0) load_symbol((uint8_t)id);
        emit(0xd1); guard(); emit(0xd5); emit(0xeb); call(RT_COMPARE);
        emit(0x7c); emit(0xb5); other=forward(0xca);
        if(id>=0) load_symbol((uint8_t)id);
        emit(newline?0x2b:0x23); if(id>=0) store_symbol((uint8_t)id);
        op16(0xc3,0x4000u+body);
        patch(branch); patch(other); emit(0xc1); /* exactly one owned bound word */
        mapping(code,pc(),start,head_end);
        --depth; return;
    } else if((is("write")||is("writeln"))&&lookup()<0) {
        newline=is("writeln"); next();
        if(token=='(') {
            next();
            do {
                kind=expression(); call(kind==K_STRING?RT_STRING:kind==K_BOOLEAN?RT_BOOLEAN:RT_INTEGER);
                if(token!=',') break;
                next();
            } while(!error->code);
            expect(')',5);
        } else if(!newline) fail(4);
        if(newline) {
            emit(0x3e); emit(13); call(RT_CHAR);
            emit(0x3e); emit(10); call(RT_CHAR);
        }
    } else if(is("readln")&&lookup()<0) {
        next(); call(RT_READLINE);
        if(token=='(') {
            next();
            do {
                id=token==T_ID?lookup():-1;
                if(id<0) { fail(41); break; }
                if(symbols[id].kind!=K_STRING||symbols[id].control) { fail(44); break; }
                op16(0x11,symbols[id].address);
                emit(0x0e); emit(symbols[id].capacity); call(RT_READSTRING);
                next(); if(token!=',') break; next();
            } while(!error->code);
            expect(')',5);
        }
    } else if((is("halt")||is("exit"))&&lookup()<0) { newline=is("halt"); next(); op16(0xc3,newline?RT_HALT:RT_DONE); }
    else if(token==T_ID&&!is("end")) {
        id=lookup();
        if(id<0) fail(41);
        else {
            if(symbols[id].control) fail(44);
            next(); expect(T_ASSIGN,7); kind=expression();
            if(kind!=symbols[id].kind) fail(44);
            if(kind==K_STRING) {
                op16(0x11,symbols[id].address); emit(0x0e); emit(symbols[id].capacity); call(RT_COPY);
            } else store_symbol((uint8_t)id);
        }
    } else if(token!=';'&&!is("end")&&!is("else")&&!is("until")) fail(54);
    mapping(code,pc(),start,token_start);
    --depth;
}
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
    uint16_t i,j,vars[32], count;
    uint8_t digest[32],capacity,kind;
    Sha256 hash;
    image=b; error=d;
    memset(d,0,sizeof(*d));
    memset(b,0,sizeof(*b)); memset(b->pages,255,3); b->map_page=255;
    if(!source_snapshot(s,&snapshot)) { d->code=98; return 0; }
    input=&snapshot;
    for(i=0;i<3;++i) {
        b->pages[i]=page_new(PAGE_IMAGE);
        if(b->pages[i]==255) { d->code=98; goto failed; }
        for(j=0;j<PAGE_SIZE;++j) far_set(b->pages[i],j,0);
        page_initialized(b->pages[i],PAGE_SIZE);
    }
    b->map_page=page_new(PAGE_MAP);
    if(b->map_page==255) { d->code=98; goto failed; }
    for(i=0;i<96;++i) map8(i,0);
    for(i=0;i<(uint16_t)(runtime_end-runtime_start);++i) put(i,runtime_start[i]);
    scan=0; symbol_count=0; depth=0; entries=0;
    file_bytes=40+(uint16_t)strlen(s->path);
    next();
    if(is("program")) {
        next(); expect(T_ID,53);
        if(token=='(') { next(); expect(T_ID,53); while(token==',') { next(); expect(T_ID,53); } expect(')',5); }
        expect(';',1);
    }
    while(is("var")&&!d->code) {
        next();
        while(token==T_ID&&!is("begin")&&!is("var")&&!d->code) {
            count=0;
            do {
                if(token!=T_ID) { fail(53); break; }
                if(lookup()>=0) { fail(43); break; }
                if(symbol_count==32) { fail(98); break; }
                strcpy(symbols[symbol_count].name,name);
                vars[count++]=symbol_count++;
                next(); if(token!=',') break; next();
            } while(!d->code);
            expect(':',2);
            capacity=0; kind=K_INTEGER;
            if(is("integer")) next();
            else if(is("boolean")) { kind=K_BOOLEAN; next(); }
            else if(is("string")) {
                kind=K_STRING;
                next(); expect('[',8);
                if(token!=T_NUMBER||magnitude<1||magnitude>255) fail(49);
                else { capacity=(uint8_t)magnitude; next(); }
                expect(']',9);
            } else fail(41);
            for(i=0;i<count;++i) {
                symbols[vars[i]].address=GLOBAL_BASE+b->globals_bytes;
                symbols[vars[i]].capacity=capacity;
                symbols[vars[i]].kind=kind;
                symbols[vars[i]].control=0;
                b->globals_bytes+=kind==K_STRING?(uint16_t)capacity+1:kind==K_BOOLEAN?1:2;
            }
            expect(';',1);
        }
    }
    b->code_origin=GLOBAL_BASE+b->globals_bytes;
    emitted=b->code_origin-0x4000u;
    if(!is("begin")) fail(12); else statement();
    expect('.',10);
    if(token!=T_EOF) fail(58);
    op16(0xc3,RT_DONE);
    if(d->code) goto failed;
    b->length=emitted; b->code_bytes=emitted-(b->code_origin-0x4000u);
    b->revision=s->revision;
    source_hash(s,b->source_hash);
    /* ZMAP: one canonical resident source, deterministic half-open mappings. */
    for(i=0;i<4;++i) map8(i,"ZMAP"[i]);
    map8(4,1); map16(6,96); map16(8,1); map16(10,b->code_origin); map16(12,b->code_bytes);
    map16(14,1); map32(16,entries); map32(84,file_bytes); map32(88,(uint32_t)entries*16);
    map16(96,0); map16(98,(uint16_t)strlen(s->path)); map32(100,snapshot.length);
    for(i=0;i<32;++i) map8(104+i,b->source_hash[i]);
    for(i=0;i<strlen(s->path);++i) map8(136+i,(uint8_t)s->path[i]);
    sha_init(&hash);
    for(i=96;i<96+file_bytes;++i) sha_byte(&hash,far_get(b->map_page,i));
    sha_end(&hash,digest);
    for(i=0;i<32;++i) { put(68+i,digest[i]); map8(52+i,digest[i]); }
    b->map_length=96+file_bytes+entries*16;
    page_initialized(b->map_page,b->map_length);
    hash_map(b,digest);
    for(i=0;i<32;++i) put(100+i,digest[i]);
    word(10,b->length); word(20,b->code_origin); word(22,b->code_bytes);
    word(24,GLOBAL_BASE); word(26,b->globals_bytes); word(28,0x4000u+b->length);
    hash_image(b,b->id);
    for(i=0;i<32;++i) { put(36+i,b->id[i]); map8(20+i,b->id[i]); }
    for(i=0;i<3;++i) b->versions[i]=page_versions[b->pages[i]];
    b->versions[3]=page_versions[b->map_page]; b->versions[4]=page_versions[s->page];
    b->valid=1;
    source_close(&snapshot);
    return 1;
failed:
    d->start=source_editor_offset(s,d->start);
    d->end=source_editor_offset(s,d->end);
    source_close(&snapshot);
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
