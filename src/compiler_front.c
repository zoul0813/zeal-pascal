#include "compiler.h"

static const char *const reserved[] = {
    "absolute","and","array","begin","case","const","div","do","downto","else","end",
    "external","file","for","forward","function","goto","if","in","inline","label","mod",
    "nil","not","of","or","overlay","packed","procedure","program","record","repeat","set",
    "shl","shr","string","then","to","type","until","var","while","with","xor"};
static const struct {const char *name;uint16_t address;uint8_t type;} system_variables[]={
    {"heapptr",RT_HEAPPTR,K_INTEGER},{"recurptr",RT_RECURPTR,K_INTEGER},
    {"buflen",RT_BUFLEN,K_BYTE},{"cbreak",RT_CBREAK,K_BOOLEAN}};
static const char *const standard_names[]={"input","output","kbd","con","trm","lst","aux","usr"};
static const char *const hook_names[]={"constptr","conoutptr","lstoutptr","auxoutptr","usroutptr"};
static uint8_t text_file_type(void) {
    if(cc.text_type==255) {
        cc.text_type=cc_new_type(K_FILE,16,0,0,FILE_TEXT);
        cc.types[cc.text_type].element=255;
    }
    return cc.text_type;
}

void cc_system_init(void) FRONTEND {
    uint8_t i;
    for(i=0;i<sizeof(system_variables)/sizeof(system_variables[0]);++i) {
        Symbol *s=&cc.symbols[cc.symbol_count++];
        memset(s,0,sizeof(*s));s->role=S_SYSTEM;
        s->type=system_variables[i].type;s->address=system_variables[i].address;
    }
    for(i=0;i<sizeof(standard_names)/sizeof(standard_names[0]);++i) {
        Symbol *s=&cc.symbols[cc.symbol_count++];
        memset(s,0,sizeof(*s));s->role=S_STANDARD;s->type=255;s->initial_type=i+1;
    }
    for(i=0;i<HOOK_COUNT;++i) {
        Symbol *s=&cc.symbols[cc.symbol_count++];
        memset(s,0,sizeof(*s));s->role=S_HOOK;s->type=K_INTEGER;s->initial_type=i;
    }
}

static uint8_t get(uint16_t p) {
    if(!++cc.scan_poll)cc_poll();
    return p<cc.input->length ? far_get(cc.input->page,p) : 0;
}
static uint8_t lower(uint8_t c) { return c>='A'&&c<='Z' ? c+32 : c; }
static uint8_t name_byte(SourcePosition position,uint8_t offset) {
    return lower(cc_source_byte(position+offset));
}
static uint8_t name_matches(uint16_t reference,uint8_t length) {
    SourcePosition position;uint8_t i;
    if(!length)return 1;
    position=cc_reference_position(reference);
    for(i=0;i<length;++i)if(cc.name[i]!=name_byte(position,i))return 0;
    return 1;
}
static uint8_t letter(uint8_t c) { c=lower(c); return (c>='a'&&c<='z')||c=='_'; }
static uint8_t digit(uint8_t c) { return c>='0'&&c<='9'; }
void cc_fail(uint8_t code) FRONTEND {
    if(!cc.error->code) {
        cc.error->code=code;cc.error->start=(uint16_t)cc.token_start;cc.error->end=(uint16_t)cc.token_end;
        cc.sources->error_file=(uint8_t)(cc.token_start>>16);
    }
}

void cc_put(uint16_t p,uint8_t v) FRONTEND {
    if(cc.error->code) return;
    if(p>=0xc000u) {
        cc_stage_put(p-0xc000u,v);
    } else {
        if(cc.image->pages[p/PAGE_SIZE]==255&&!cc_image_page(p/PAGE_SIZE))return;
        far_set(cc.image->pages[p/PAGE_SIZE],p%PAGE_SIZE,v);
    }
}
uint8_t cc_code_get(uint16_t address) FRONTEND {
    if(cc.error->code) return 0;
    if(address<CC_CODE_BASE||address>=cc.emitted) { cc_fail(99); return 0; }
    return address>=0xc000u?cc_stage_get(address-0xc000u):
        far_get(cc.image->pages[2],address-CC_CODE_BASE);
}
void cc_word(uint16_t p,uint16_t v) FRONTEND { cc_put(p,(uint8_t)v); cc_put(p+1,(uint8_t)(v>>8)); }



void cc_emit(uint8_t v) FRONTEND {
    if(cc.error->code||cc.suppress) return;
    if(!(cc.emitted&255)&&!cc_poll())return;
    /* The first staging page reuses image page2. A second owned compiler page
     * is acquired only if necessary, then released after checked placement. */
    if(cc.emitted>=CC_STATIC_LIMIT) { cc_fail(99); return; }
    cc_put(cc.emitted++,v);
}
static void emit16(uint16_t v) { cc_emit((uint8_t)v); cc_emit((uint8_t)(v>>8)); }
void cc_op16(uint8_t op,uint16_t value) FRONTEND { cc_emit(op); emit16(value); }
void cc_relocate(uint16_t operand) FRONTEND {
    cc_relocate_value(operand,1,0);
}
void cc_relocate_value(uint16_t operand,uint16_t coefficient,uint8_t byte) FRONTEND {
    uint16_t p=cc.relocations*RELOCATION_BYTES,encoded;
    if(cc.error->code||cc.suppress||!coefficient)return;
    if(operand<CC_CODE_BASE||operand>=CC_STATIC_LIMIT||
       (uint32_t)p+RELOCATION_BYTES+(uint32_t)cc.routine_count*ROUTINE_BYTES>PAGE_SIZE) { cc_fail(99);return; }
    /* Staged offsets are below7400. Bit15 denotes one-byte relocation;
     * the other word is the net code-base coefficient, modulo65536. */
    encoded=(operand-CC_CODE_BASE)|(byte?0x8000u:0);
    far_set(cc.workspace,p,(uint8_t)encoded);far_set(cc.workspace,p+1,(uint8_t)(encoded>>8));
    far_set(cc.workspace,p+2,(uint8_t)coefficient);far_set(cc.workspace,p+3,(uint8_t)(coefficient>>8));
    ++cc.relocations;
}
void cc_address(uint8_t op,uint16_t value) FRONTEND {
    cc_relocate(cc.emitted+1); cc_op16(op,value);
}

uint16_t cc_forward(uint8_t op) FRONTEND { uint16_t at=cc.emitted+1; cc_address(op,0); return at; }
void cc_patch(uint16_t at) FRONTEND { if(!cc.error->code&&!cc.suppress) cc_word(at,cc.emitted); }
uint16_t cc_link(uint8_t opcode,uint16_t head) FRONTEND {
    if(cc.suppress) return head;
    uint16_t at=cc.emitted+1; cc_address(opcode,head); return at;
}
void cc_patch_chain(uint16_t head,uint16_t value) FRONTEND {
    uint16_t next;
    while(head&&!cc.error->code) {
        next=cc_code_get(head)|((uint16_t)cc_code_get(head+1)<<8);
        cc_word(head,value); head=next;
    }
}
void cc_reserve(uint16_t bytes) FRONTEND {
    uint16_t pc=cc.emitted-CC_CODE_BASE;
    /* Runtime consumes inline size/PC words and preserves both register sets. */
    cc_op16(0xcd,RT_GUARD);
    cc_emit((uint8_t)bytes); cc_emit((uint8_t)(bytes>>8));
    cc_emit((uint8_t)pc); cc_emit((uint8_t)(pc>>8));
}
void cc_guard(void) FRONTEND { cc_reserve(0); }
/* Preserve the actual caller PC and every operand, including an optional A
 * character. Self-modified immediates are consumed before any callback runs. */
void cc_capture_guard(uint16_t bytes,uint8_t a) FRONTEND {
    uint16_t start=cc.emitted;
    if(a) cc_address(0x32,start+23);
    start=cc.emitted;
    cc_op16(0x3a,RT_PC); cc_address(0x32,start+17);
    cc_op16(0x3a,RT_PC+1); cc_address(0x32,start+18);
    cc_reserve(bytes);
    if(a) { cc_emit(0x3e); cc_emit(0); }
}
uint16_t cc_original_entry(uint16_t entry,uint8_t staged) FRONTEND {
    uint8_t i;
    uint16_t at=cc.emitted,n,encoded,coefficient,count=cc.relocations;
    for(i=0;i<3;++i) cc_emit(staged?cc_code_get(entry+i):cc_runtime_get(entry-0x4000u+i));
    /* These adapters begin with one three-byte instruction. Its operand may
     * name generated self-modifying guard storage, not just fixed prefix
     * data. A copied instruction retains the source operand's relocation. */
    if(staged)for(n=0;n<count;++n) {
        encoded=far_get(cc.workspace,n*RELOCATION_BYTES)|((uint16_t)far_get(cc.workspace,n*RELOCATION_BYTES+1)<<8);
        if((encoded&0x7fffu)+CC_CODE_BASE==entry+1) {
            coefficient=far_get(cc.workspace,n*RELOCATION_BYTES+2)|((uint16_t)far_get(cc.workspace,n*RELOCATION_BYTES+3)<<8);
            cc_relocate_value(at+1,coefficient,(encoded&0x8000u)!=0);break;
        }
    }
    if(staged) cc_address(0xc3,entry+3); else cc_op16(0xc3,entry+3);
    return at;
}
/* Generated private helpers preserve complete Pascal CPU state around polls. */
void cc_preserve_registers(uint8_t restore) FRONTEND {
    static const uint8_t save[]={0xf5,0xc5,0xd5,0xe5,0xdd,0xe5,0xfd,0xe5,
        0x08,0xf5,0x08,0xd9,0xc5,0xd5,0xe5,0xd9};
    static const uint8_t load[]={0xd9,0xe1,0xd1,0xc1,0xd9,0x08,0xf1,0x08,
        0xfd,0xe1,0xdd,0xe1,0xe1,0xd1,0xc1,0xf1};
    uint8_t i;for(i=0;i<16;++i)cc_emit(restore?load[i]:save[i]);
}
/* Terminal entries restore their own emergency stack before doing cleanup.
 * Capturing their source PC must not push onto a caller-selected boundary SP. */
void cc_finish(uint16_t entry) FRONTEND {
    cc_op16(0x21,cc.emitted-CC_CODE_BASE);
    cc_op16(0x22,RT_PC);
    cc_emit(0x06); cc_emit(cc.io_check); cc_op16(0xc3,entry);
}
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
    if(cc_family(type)==K_FILE) { cc_fail(54); return; }
    if(cc_family(type)==K_SET) { cc_set_load(type); return; }
    if(cc_family(type)==K_STRING||cc_family(type)==K_REAL||cc_aggregate(type)) return;
    if(type==255) { cc_fail(44); return; }
    if(cc.types[type].width==1) { cc_emit(0x6e); cc_emit(0x26); cc_emit(0); }
    else { cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb); }
}
void cc_store_indirect(uint8_t type) FRONTEND {
    cc_emit(0x7d); cc_emit(0x12);
    if(cc.types[type].width==2) { cc_emit(0x13); cc_emit(0x7c); cc_emit(0x12); }
}
void cc_call(uint16_t address) FRONTEND { cc_guard(); cc_op16(0xcd,address); }
static void literal(void) {
    uint8_t c,base,value;
    uint16_t n;
    uint32_t wide;
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
    cc.token=T_STRING; cc.token_end=cc_source_position();
}
void cc_control_literal(void) FRONTEND {
    cc.scan=(uint16_t)cc.token_start; cc.literal_length=0; literal();
}
void cc_next(void) FRONTEND {
    uint8_t c,q,base,value;
    uint16_t n=0;
    uint32_t wide;
    if(cc.error->code||!cc_poll()) { cc.token=T_EOF; return; }
    for(;;) {
        if(cc.error->code) { cc.token=T_EOF; return; }
        if(cc.scan==cc.input->length&&cc.sources->file&&cc_source_resume())continue;
        c=get(cc.scan);
        if(cc.scan<cc.input->length && c<=32) { ++cc.scan; continue; }
        if(c=='{' || (c=='('&&get(cc.scan+1)=='*')) {
            q=c; cc.scan+=q=='{' ? 1 : 2;
            if(get(cc.scan)=='$') {
                cc.token_start=cc_source_position();cc.token_end=cc.token_start+1;++cc.scan;
                if(lower(get(cc.scan))=='i'&&get(cc.scan+1)!='+'&&get(cc.scan+1)!='-') {
                    cc_source_include(q);
                    if(cc.error->code){cc.token=T_EOF;return;}
                    continue;
                }
                do {
                    c=lower(get(cc.scan)); value=get(cc.scan+1)=='+';
                    if(c=='w'&&digit(get(cc.scan+1))) cc.with_limit=get(cc.scan+1)-'0';
                    else if((c!='r'&&c!='a'&&c!='v'&&c!='b'&&c!='i'&&c!='x'&&c!='u'&&c!='c')||(get(cc.scan+1)!='+'&&get(cc.scan+1)!='-')) {
                        cc_fail(93); cc.token=T_EOF; return;
                    }
                    else if(c=='r') cc.range_check=value;
                    else if(c=='a') cc.absolute_mode=value;
                    else if(c=='v') cc.var_strict=value;
                    else if(c=='b') cc.input_fresh=value;
                    else if(c=='i') cc.io_check=value;
                    else if(c=='x') cc.index_speed=value;
                    else if(c=='u') cc.user_interrupt=value;
                    else if(c=='c') cc.console_control=value;
                    cc.scan+=2;
                    if(get(cc.scan)!=',') break;
                    ++cc.scan;
                } while(!cc.error->code);
                while(get(cc.scan)==' '||get(cc.scan)=='\t') ++cc.scan;
                if(!(q=='{'?get(cc.scan)=='}':get(cc.scan)=='*'&&get(cc.scan+1)==')')) {
                    cc_fail(93); cc.token=T_EOF; return;
                }
            }
            while(!cc.error->code&&cc.scan<cc.input->length && !(q=='{' ? get(cc.scan)=='}' : get(cc.scan)=='*'&&get(cc.scan+1)==')')) ++cc.scan;
            if(cc.scan==cc.input->length) { cc.token_start=cc_source_position();cc.token_end=cc.token_start;cc_fail(91);cc.token=T_EOF;return; }
            cc.scan+=q=='{' ? 1 : 2; continue;
        }
        break;
    }
    cc.token_start=cc_source_position();cc.token_end=cc.token_start+1;cc.hexadecimal=0;cc.number_overflow=0;cc.literal_length=0;
    if(cc.scan==cc.input->length) { cc.token=T_EOF; cc.token_end=cc.token_start; return; }
    if(letter(c)) {
        do {
            if(n<127) cc.name[n++]=lower(c);
            /* Keep the bounded byte poll in get, including lookahead. */
            c=get(++cc.scan);
        } while(letter(c)||digit(c));
        cc.name[n]=0; cc.token=T_ID;
        for(n=0;n<sizeof(reserved)/sizeof(reserved[0]);++n)
            if(!strcmp(cc.name,reserved[n])) { cc.token=T_RESERVED; break; }
    } else if(digit(c)||c=='$') {
        base=c=='$'?16:10; cc.hexadecimal=base==16;
        if(cc.hexadecimal) ++cc.scan;
        wide=0; n=0;
        while(digit(get(cc.scan))||(base==16&&lower(get(cc.scan))>='a'&&lower(get(cc.scan))<='f')) {
            c=lower(get(cc.scan++)); value=digit(c)?c-'0':c-'a'+10;
            if(n<127) cc.name[n]=(char)c;
            ++n;
            if(!cc.number_overflow) {
                wide=wide*base+value;
                if(wide>(base==16?65535UL:32768UL)) cc.number_overflow=1;
            }
            if(base==16&&n>4) { cc_fail(56); break; }
        }
        if(!n) cc_fail(56);
        cc.name[n<127?n:127]=0;
        cc.magnitude=(uint16_t)wide; cc.token=T_NUMBER;
        if(base==10&&((get(cc.scan)=='.'&&get(cc.scan+1)!='.'&&get(cc.scan+1)!=')')||lower(get(cc.scan))=='e')) {
            if(get(cc.scan)=='.') {
                ++cc.scan; while(digit(get(cc.scan))) ++cc.scan;
            }
            if(lower(get(cc.scan))=='e') {
                ++cc.scan;
                if(get(cc.scan)=='+'||get(cc.scan)=='-') ++cc.scan;
                if(!digit(get(cc.scan))) cc_fail(57);
                if(digit(get(cc.scan))) ++cc.scan;
                if(digit(get(cc.scan))) ++cc.scan;
            }
            if(get(cc.scan)=='.') cc_fail(57);
            cc.token=T_REAL;
        }
    } else if(c=='\''||c=='#') {
        literal();
    } else {
        ++cc.scan; cc.token=c;
        if(!strchr(";:.,()+-*/[]=<>^",c)) cc_fail(58);
        if(c==':'&&get(cc.scan)=='=') { ++cc.scan; cc.token=T_ASSIGN; }
        if(c=='<'&&get(cc.scan)=='>') { ++cc.scan; cc.token=T_NE; }
        if(c=='<'&&get(cc.scan)=='=') { ++cc.scan; cc.token=T_LE; }
        if(c=='>'&&get(cc.scan)=='=') { ++cc.scan; cc.token=T_GE; }
        if(c=='.'&&get(cc.scan)=='.') { ++cc.scan; cc.token=T_RANGE; }
        if(c=='('&&get(cc.scan)=='.') { ++cc.scan; cc.token='['; }
        if(c=='.'&&get(cc.scan)==')') { ++cc.scan; cc.token=']'; }
    }
    cc.token_end=cc_source_position();
}

void cc_expect(uint16_t t,uint8_t code) FRONTEND { if(cc.token!=t) cc_fail(code); else cc_next(); }

int16_t cc_lookup(void) FRONTEND {
    uint16_t i; int16_t field; uint8_t j,scope=cc.scope,n=(uint8_t)strlen(cc.name);
    for(j=cc.with_depth;j;--j) {
        field=cc_field_lookup(cc.with_types[j-1]);
        if(field>=0) return field;
    }
    for(;;) {
        i=cc.symbol_count;
        while(i--) if(cc.symbols[i].role<S_FIELD&&cc.symbols[i].owner==scope) {
            if(cc.symbols[i].role==S_STANDARD) {
                if(cc_is(standard_names[cc.symbols[i].initial_type-1])) {
                    cc.symbols[i].type=text_file_type(); return (int16_t)i;
                }
                continue;
            }
            if(cc.symbols[i].role==S_HOOK) {
                if(cc_is(hook_names[cc.symbols[i].initial_type])) return (int16_t)i;
                continue;
            }
            if(cc.symbols[i].role==S_SYSTEM) {
                for(j=0;j<sizeof(system_variables)/sizeof(system_variables[0]);++j)
                    if(cc.symbols[i].address==system_variables[j].address&&cc_is(system_variables[j].name)) return (int16_t)i;
                continue;
            }
            if(cc.symbols[i].name_length!=n) continue;
            if(name_matches(cc.symbols[i].name_start,n)) return (int16_t)i;
        }
        if(!scope) break;
        scope=(uint8_t)cc_rget(scope,R_PARENT);
    }
    return -1;
}
static int16_t find_field(uint8_t type,uint16_t field) {
    uint16_t i,variant;
    uint8_t n=(uint8_t)strlen(cc.name);
    int16_t found;
    for(i=0;i<cc.symbol_count;++i) if(cc.symbols[i].role==S_FIELD&&cc.symbols[i].owner==type) {
        if(field!=0xffffu) { if(field!=i) continue; }
        else {
            if(cc.symbols[i].name_length!=n) continue;
            if(!name_matches(cc.symbols[i].name_start,n)) continue;
        }
        cc.field_offset=cc.symbols[i].address; return (int16_t)i;
    }
    if(cc.types[type].low<0) return -1;
    variant=(uint16_t)cc.types[type].low;
    for(i=0;i<cc.symbol_count;++i) if(cc.symbols[i].role==S_VLABEL&&cc.symbols[i].owner==type) {
        found=find_field(cc.symbols[i].type,field);
        if(found>=0) { cc.field_offset+=cc.symbols[variant].address; return found; }
    }
    return -1;
}
int16_t cc_field_lookup(uint8_t type) FRONTEND { return find_field(type,0xffffu); }
uint16_t cc_field_path(uint8_t type,uint16_t field) FRONTEND {
    return find_field(type,field)<0?0xffffu:cc.field_offset;
}
static void previous_with(void) {
    cc_emit(0x23); cc_emit(0x23); cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
}
void cc_with_address(uint16_t field) FRONTEND {
    uint8_t i=cc.with_depth,distance=0;
    uint16_t offset=0xffffu;
    while(i) { offset=cc_field_path(cc.with_types[--i],field); if(offset!=0xffffu) break; ++distance; }
    if(offset==0xffffu) { cc_fail(41); return; }
    cc_guard(); cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe1); /* HL = current With frame */
    while(distance--) previous_with();
    cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
    if(offset) { cc_op16(0x11,offset); cc_emit(0x19); }
}
void cc_with_unwind(void) FRONTEND {
    uint8_t count=cc.with_depth;
    if(!count) return;
    cc_guard(); cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe1);
    while(count--) previous_with();
    cc_emit(0xe5); cc_emit(0xfd); cc_emit(0xe1);
}
uint16_t cc_new_symbol(uint8_t role) FRONTEND {
    uint16_t id=cc.symbol_count;
    int16_t old=cc_lookup();
    if(cc.token!=T_ID) cc_fail(53);
    else if(old>=0&&cc.symbols[old].owner==cc.scope&&cc.symbols[old].role!=S_SYSTEM&&cc.symbols[old].role!=S_STANDARD&&cc.symbols[old].role!=S_HOOK) cc_fail(43);
    else if(id==SYMBOL_LIMIT) cc_fail(98);
    if(cc.error->code) return 0;
    memset(&cc.symbols[id],0,sizeof(Symbol));
    cc.symbols[id].name_start=cc_source_reference(cc.token_start);cc.symbols[id].name_length=(uint8_t)strlen(cc.name);
    cc.symbols[id].role=role; cc.symbols[id].owner=cc.scope; ++cc.symbol_count;
    return id;
}
uint8_t cc_new_type(uint8_t family,uint16_t width,int16_t low,int16_t high,uint8_t capacity) FRONTEND {
    uint8_t id=cc.type_count;
    if(id==TYPE_LIMIT) { cc_fail(98); return 0; }
    cc.types[id].family=family; cc.types[id].width=width;
    cc.types[id].low=low; cc.types[id].high=high; cc.types[id].capacity=capacity;
    cc.types[id].element=0;
    ++cc.type_count; return id;
}
uint8_t cc_family(uint8_t type) FRONTEND {
    if(type>=K_SET_EXPR&&type<=K_EMPTY_SET) return K_SET;
    if(type==K_ANY_POINTER||type==K_NIL) return K_POINTER;
    return type<cc.type_count?cc.types[type].family:255;
}
static uint8_t text_type(uint8_t type) { return cc_family(type)==K_CHAR||cc_family(type)==K_STRING; }
uint8_t cc_compatible(uint8_t left,uint8_t right) FRONTEND {
    if(cc_family(left)==K_FILE||cc_family(right)==K_FILE)
        return cc_family(left)==K_FILE&&cc_family(right)==K_FILE&&
            cc.types[left].capacity==cc.types[right].capacity&&
            (cc.types[left].capacity!=FILE_TYPED||cc.types[left].element==cc.types[right].element);
    if(cc_family(left)==K_REAL&&cc_family(right)==K_INTEGER) return 1;
    if(cc_family(left)==K_POINTER||cc_family(right)==K_POINTER) {
        if(cc_family(left)!=K_POINTER||cc_family(right)!=K_POINTER) return 0;
        return left==K_ANY_POINTER||right==K_ANY_POINTER||left==K_NIL||right==K_NIL||
            cc.types[left].element==cc.types[right].element;
    }
    if(cc_family(left)==K_SET||cc_family(right)==K_SET) {
        if(cc_family(left)!=K_SET||cc_family(right)!=K_SET) return 0;
        return cc_set_base(left)==255||cc_set_base(right)==255||cc_set_base(left)==cc_set_base(right);
    }
    if(cc_aggregate(left)||cc_aggregate(right))
        return left==right||((cc_char_array(left)||text_type(left))&&(cc_char_array(right)||text_type(right)));
    return cc_family(left)==cc_family(right)||(text_type(left)&&text_type(right));
}
uint8_t cc_variable(int16_t id) FRONTEND {
    return id>=0&&(cc.symbols[id].role==S_VARIABLE||cc.symbols[id].role==S_TYPED||
           cc.symbols[id].role==S_VALUE||cc.symbols[id].role==S_REFERENCE||cc.symbols[id].role==S_RESULT||cc.symbols[id].role==S_FIELD||cc.symbols[id].role==S_SYSTEM||
           cc.symbols[id].role==S_ADDRESS||cc.symbols[id].role==S_ALIAS||cc.symbols[id].role==S_STANDARD||cc.symbols[id].role==S_HOOK);
}
uint16_t cc_rget(uint8_t scope,uint8_t field) FRONTEND {
    uint16_t at=PAGE_SIZE-(uint16_t)scope*ROUTINE_BYTES+field;
    uint16_t value=far_get(cc.workspace,at);
    if(field>R_FLAGS) value|=(uint16_t)far_get(cc.workspace,at+1)<<8;
    return value;
}
void cc_rput(uint8_t scope,uint8_t field,uint16_t value) FRONTEND {
    uint16_t at=PAGE_SIZE-(uint16_t)scope*ROUTINE_BYTES+field;
    far_set(cc.workspace,at,(uint8_t)value);
    if(field>R_FLAGS) far_set(cc.workspace,at+1,(uint8_t)(value>>8));
}
void cc_storage(uint16_t id,uint16_t width) FRONTEND {
    uint16_t count;
    if(cc.scope&&(cc_rget(cc.scope,R_FLAGS)&RF_RECURSIVE)) {
        count=cc_rget(cc.scope,R_LOCALS);
        if((uint32_t)count+width>0x7ff0u) { cc_fail(99); return; }
        count+=width; cc_rput(cc.scope,R_LOCALS,count);
        cc.symbols[id].address=(uint16_t)(0u-FRAME_PRIVATE_BYTES-count);
    } else {
        if((uint32_t)cc.image->globals_bytes+width>=PAGE_SIZE) { cc_fail(99); return; }
        cc.symbols[id].address=GLOBAL_BASE+cc.image->globals_bytes;
        cc.image->globals_bytes+=width;
        if(cc.symbols[id].role==S_VARIABLE)
            cc_file_static(cc.symbols[id].type,cc.symbols[id].address);
    }
}
uint8_t cc_frame_required(uint8_t scope) FRONTEND {
    while(scope) {
        if(cc_rget(scope,R_FLAGS)&RF_RECURSIVE) return 1;
        scope=(uint8_t)cc_rget(scope,R_PARENT);
    }
    return 0;
}
void cc_frame(uint8_t scope) FRONTEND {
    uint8_t current=cc.scope;
    if(!scope||(scope!=cc.scope&&!cc_frame_required(scope))) { cc_op16(0x21,0); return; }
    cc_guard(); cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xe1); /* HL = IX */
    while(current!=scope&&!cc.error->code) {
        if(!current) { cc_fail(44); return; }
        cc_emit(0x2b); cc_emit(0x2b); cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
        current=(uint8_t)cc_rget(current,R_PARENT);
    }
}
void cc_bind_storage(uint16_t id) FRONTEND {
    Symbol *s=&cc.symbols[id];
    if(s->role==S_HOOK) cc_hook_variable(id);
    if(s->role==S_STANDARD&&!s->address&&!cc.suppress&&!cc.error->code) {
        if((uint32_t)cc.image->globals_bytes+16>=PAGE_SIZE) { cc_fail(99); return; }
        s->type=text_file_type();
        s->address=GLOBAL_BASE+cc.image->globals_bytes; cc.image->globals_bytes+=16;
        cc_file_static(s->type,s->address); cc_put(s->address-0x4000u+9,1);
    }
}
void cc_slot(uint16_t id) FRONTEND {
    Symbol *s=&cc.symbols[id];
    cc_bind_storage(id);
    if(s->role==S_ALIAS) cc_symbol_address(s->address);
    else if(s->role==S_ADDRESS) cc_op16(0x21,s->address);
    else if(s->role==S_TYPED) cc_address(0x21,s->address);
    else if(s->owner&&(cc_rget(s->owner,R_FLAGS)&RF_RECURSIVE)) {
        cc_frame(s->owner); cc_op16(0x11,s->address); cc_emit(0x19);
    } else cc_op16(0x21,s->address);
}
uint8_t cc_standard_reference(uint8_t identity) FRONTEND {
    uint16_t i;
    for(i=0;i<cc.symbol_count;++i) if(cc.symbols[i].role==S_STANDARD&&cc.symbols[i].initial_type==identity) {
        cc.symbols[i].type=text_file_type(); cc_slot(i); return cc.symbols[i].type;
    }
    cc_fail(99); return 255;
}
void cc_symbol_address(uint16_t id) FRONTEND {
    if(cc.symbols[id].role==S_FIELD) { cc_with_address(id); return; }
    cc_slot(id);
    if(cc.symbols[id].role==S_REFERENCE) { cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb); }
}
static uint8_t indirect_symbol(const Symbol *s) {
    return s->role==S_ALIAS||s->role==S_FIELD||s->role==S_REFERENCE||
        (s->role!=S_ADDRESS&&s->owner&&(cc_rget(s->owner,R_FLAGS)&RF_RECURSIVE));
}
void cc_destination(uint16_t id) FRONTEND {
    Symbol *s=&cc.symbols[id];
    if(s->role==S_HOOK) cc_hook_variable(id);
    if(indirect_symbol(s)) {
        cc_guard(); cc_emit(0xe5); cc_symbol_address(id); cc_emit(0xeb); cc_emit(0xe1);
    } else if(s->role==S_TYPED) cc_address(0x11,s->address);
    else cc_op16(0x11,cc.symbols[id].address);
}
void cc_load_symbol(uint16_t id) FRONTEND {
    Symbol *s=&cc.symbols[id];
    if(s->role==S_HOOK) cc_hook_variable(id);
    if(s->type==255) { cc_fail(44); return; }
    if(cc_family(s->type)==K_FILE) { cc_fail(54); return; }
    if(cc_family(s->type)==K_SET) { cc_symbol_address(id); cc_set_load(s->type); return; }
    if(cc_family(s->type)==K_STRING||cc_family(s->type)==K_REAL||cc_aggregate(s->type)) cc_symbol_address(id);
    else if(indirect_symbol(s)) {
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
    if(s->role==S_HOOK) cc_hook_variable(id);
    if(s->type==255) { cc_fail(44); return; }
    op=cc.types[s->type].width==1?0x32:0x22;
    if(indirect_symbol(s)) {
        cc_guard(); cc_emit(0xe5); cc_symbol_address(id); cc_emit(0xd1); cc_emit(0x73);
        if(op==0x22) { cc_emit(0x23); cc_emit(0x72); }
        return;
    }
    if(op==0x32) cc_emit(0x7d);
    if(cc.symbols[id].role==S_TYPED) cc_address(op,cc.symbols[id].address); else cc_op16(op,cc.symbols[id].address);
}
void cc_check_range(uint8_t type) FRONTEND {
    if(type==255) { cc_fail(44); return; }
    if(!cc.range_check||!cc_ordinal(type)) return;
    cc_op16(0x11,(uint16_t)cc.types[type].low); cc_op16(0x01,(uint16_t)cc.types[type].high); cc_call(RT_RANGE);
}
void cc_result_symbol(uint16_t routine) FRONTEND {
    uint16_t i; uint8_t j;SourcePosition left,right;
    Symbol *s=&cc.symbols[routine];
    for(i=0;i<cc.symbol_count;++i) if(cc.symbols[i].role<S_FIELD&&cc.symbols[i].owner==cc.scope&&cc.symbols[i].name_length==s->name_length) {
        j=0;
        if(s->name_length) {
            left=cc_reference_position(cc.symbols[i].name_start);right=cc_reference_position(s->name_start);
            for(;j<s->name_length;++j)if(name_byte(left,j)!=name_byte(right,j)) break;
        }
        if(j==s->name_length) { cc_fail(43); return; }
    }
    if(cc.symbol_count==SYMBOL_LIMIT) { cc_fail(98); return; }
    i=cc.symbol_count++; cc.symbols[i]=*s;
    cc.symbols[i].owner=cc.scope; cc.symbols[i].role=S_RESULT;
    cc_storage(i,cc.types[s->type].width);
}
void cc_convert(uint8_t target,uint8_t source) FRONTEND {
    if(cc_aggregate(target)) { if(target!=source) cc_fail(44); return; }
    if(target==255||source==255||!cc_compatible(target,source)) { cc_fail(44); return; }
    source=cc_text_value(source);
    if(cc_family(target)==K_REAL&&cc_family(source)==K_INTEGER) cc_real_unary(ZR_FROM_INT,source,cc.temporaries);
    if(cc_family(target)==K_CHAR&&cc_family(source)==K_STRING) cc_call(RT_STRINGCHAR);
    cc_check_range(target);
}
void cc_set_load(uint8_t type) FRONTEND {
    cc_temporary(32);
    cc_emit(0x3e); cc_emit((uint8_t)(cc.types[cc.types[type].element].low>>3));
    cc_emit(0x0e); cc_emit((uint8_t)cc.types[type].width); cc_call(RT_SETLOAD);
}
void cc_set_store(uint8_t type) FRONTEND {
    cc_emit(0x3e); cc_emit((uint8_t)(cc.types[cc.types[type].element].low>>3));
    cc_emit(0x0e); cc_emit((uint8_t)cc.types[type].width); cc_call(RT_SETSTORE);
}
uint8_t cc_text_value(uint8_t type) FRONTEND {
    if(!cc_char_array(type)) return type;
    cc_temporary(256); cc_emit(0x3e); cc_emit((uint8_t)cc.types[type].width);
    cc_emit(0x12); cc_emit(0x13); cc_op16(0x01,cc.types[type].width); cc_call(RT_MOVE);
    cc_op16(0x21,0); cc_emit(0x39); return K_STRING;
}
/* String constants retain their source location, not a pointer into compiler
 * scratch memory. Re-decode only that literal while preserving parser state. */
void cc_constant_literal(uint16_t at) FRONTEND {
    SourceCursor cursor;
    cc_source_save(&cursor);cc_source_seek(cc_reference_position(at));
    cc_next();if(cc.token=='^')cc_control_literal();
    /* Preserve the decoded literal result; re-lexing the caller would replace
     * it. Reading one literal cannot change directives or the include frame. */
    if(cc.error->code)return;
    cc_source_seek(((SourcePosition)cursor.file<<16)|cursor.scan);
    cc.token=cursor.token;cc.token_start=cursor.first;cc.token_end=cursor.last;
}
void cc_emit_literal(void) FRONTEND {
    uint16_t i,address=cc.emitted+3;
    cc_address(0xc3,address+cc.literal_length+1); cc_emit((uint8_t)cc.literal_length);
    for(i=0;i<cc.literal_length;++i) cc_emit(cc.literal[i]);
    cc_address(0x21,address);
}
int16_t cc_predefined_type(void) FRONTEND {
    if(cc_is("text")) {
        /* Text is predefined but lazy, preserving the existing type capacity
         * and enum identities for programs which do not use Pascal files. */
        return text_file_type();
    }
    return cc_is("integer")?K_INTEGER:cc_is("boolean")?K_BOOLEAN:cc_is("char")?K_CHAR:cc_is("byte")?K_BYTE:cc_is("real")?K_REAL:-1;
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
    if(cc.token=='^') cc_control_literal();
    *type=K_INTEGER; *value=0;
    if(cc.token==T_NUMBER) {
        if(cc.number_overflow||(!cc.hexadecimal&&cc.magnitude>32767u&&sign!=2)) cc_fail(56);
        *value=cc.magnitude;
    } else if(cc.token==T_REAL) {
        *type=K_REAL;*value=cc_source_reference(cc.token_start);cc_real_literal(*value);
    } else if(cc.token==T_STRING&&!sign) {
        *type=cc.literal_length==1?K_CHAR:K_STRING;
        *value=cc.literal_length==1?cc.literal[0]:cc_source_reference(cc.token_start);
    } else if(cc.token==T_ID) {
        id=cc_lookup();
        if(id>=0&&cc.symbols[id].role==S_CONSTANT) { *type=cc.symbols[id].type; *value=cc.symbols[id].address; }
        else if(id<0&&(cc_is("true")||cc_is("false"))) { *type=K_BOOLEAN; *value=cc_is("true"); }
        else if(id<0&&cc_is("maxint")) *value=32767;
        else if(id<0&&cc_is("pi")) { *type=K_REAL; *value=REAL_PI; }
        else cc_fail(41);
    } else cc_fail(25);
    if(sign&&!cc_numeric(*type)) cc_fail(25);
    if(sign==2) *value=cc_family(*type)==K_REAL?*value^0x8000u:(uint16_t)(0u-*value);
    cc_next();
}
