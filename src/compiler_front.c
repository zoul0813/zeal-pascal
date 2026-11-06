#include "compiler.h"
#include "compiler_disk.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "compiler_units.h"
#endif

static const char *const reserved[] = {
    "absolute","and","array","begin","case","const","div","do","downto","else","end",
    "external","file","for","forward","function","goto","if","in","inline","label","mod",
    "nil","not","of","or","overlay","packed","procedure","program","record","repeat","set",
    "shl","shr","string","then","to","type","until","var","while","with","xor"};
static const struct {const char *name;uint16_t address;TypeId type;} system_variables[]={
    {"heapptr",RT_HEAPPTR,K_INTEGER},{"recurptr",RT_RECURPTR,K_INTEGER},
    {"buflen",RT_BUFLEN,K_BYTE},{"cbreak",RT_CBREAK,K_BOOLEAN}};
static const char *const standard_names[]={"input","output","kbd","con","trm","lst","aux","usr"};
static const char *const hook_names[]={"constptr","conoutptr","lstoutptr","auxoutptr","usroutptr"};
static TypeId text_file_type(void) {
    if(cc.text_type==TYPE_NONE) {
        cc.text_type=cc_new_type(K_FILE,176,0,0,FILE_TEXT);
        CCT_SET(cc.text_type,element,TYPE_NONE);
    }
    return cc.text_type;
}

void cc_system_init(void) FRONTEND {
    uint8_t i;
    for(i=0;i<sizeof(system_variables)/sizeof(system_variables[0]);++i) {
        uint16_t id=cc.symbol_count++;
        cc_symbol_clear(id);CCS_SET(id,role,S_SYSTEM);
        CCS_SET(id,type,system_variables[i].type);CCS_SET(id,address,system_variables[i].address);
    }
    for(i=0;i<sizeof(standard_names)/sizeof(standard_names[0]);++i) {
        uint16_t id=cc.symbol_count++;
        cc_symbol_clear(id);CCS_SET(id,role,S_STANDARD);CCS_SET(id,type,TYPE_NONE);CCS_SET(id,initial_type,i+1);
    }
    for(i=0;i<HOOK_COUNT;++i) {
        uint16_t id=cc.symbol_count++;
        cc_symbol_clear(id);CCS_SET(id,role,S_HOOK);CCS_SET(id,type,K_INTEGER);CCS_SET(id,initial_type,i);
    }
}

static uint8_t get(uint16_t p) {
    if(!++cc.scan_poll)cc_poll();
    return p<cc.input->length ? buffer_view_get(cc.input->page,cc.sources->input_base,p) : 0;
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
/* One lookup sees immutable captured inputs. Validate a source once for each
 * file change and retain only this call's near descriptor, never a mapped
 * pointer. Field lookup keeps its existing independent name comparator. */
static uint8_t name_matches_view(uint16_t reference,uint8_t length,Source *view,uint16_t *loaded,uint16_t *base) {
    SourcePosition position;uint16_t at;uint8_t file,i;
    if(!length)return 1;
    position=cc_reference_position(reference);file=(uint8_t)(position>>16);at=(uint16_t)position;
    if(cc.error->code)return 0;
    if(*loaded!=file) {
        if(!sources_ready(&cc.image->sources,file)||!sources_get(&cc.image->sources,file,view))return 0;
        *loaded=file;*base=sources_offset(&cc.image->sources,file);
    }
    if((uint32_t)at+length>view->length)return 0;
    for(i=0;i<length;++i)if(cc.name[i]!=lower(buffer_view_get(view->page,*base,at+i)))return 0;
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

void cc_relocate(uint16_t operand) FRONTEND {
    cc_relocate_value(operand,1,0);
}
void cc_relocate_value(uint16_t operand,uint16_t coefficient,uint8_t byte) FRONTEND {
    uint16_t p=cc.relocations*RELOCATION_BYTES,encoded;
    if(cc.error->code||cc.suppress||!coefficient)return;
    if(operand<CC_CODE_BASE||operand>=CC_STATIC_LIMIT||
       (uint32_t)p+RELOCATION_BYTES>SOURCE_LIMIT) { cc_fail(99);return; }
    /* Staged offsets are below7400. Bit15 denotes one-byte relocation;
     * the other word is the net code-base coefficient, modulo65536. */
    encoded=(operand-CC_CODE_BASE)|(byte?0x8000u:0);
    cc_reloc_put(p,(uint8_t)encoded);cc_reloc_put(p+1,(uint8_t)(encoded>>8));
    cc_reloc_put(p+2,(uint8_t)coefficient);cc_reloc_put(p+3,(uint8_t)(coefficient>>8));
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
    /* Include arguments already pushed while evaluating subsequent/nested
     * arguments. Larger caller frames retain independent memory windows. */
    if((uint32_t)bytes+cc.temporaries+cc.arguments>512u)cc_require_full_memory();
    uint16_t pc=cc.emitted-CC_CODE_BASE;
    /* Runtime consumes inline size/PC words and preserves both register sets. */
    cc_op16(0xcd,RT_GUARD);
    cc_emit((uint8_t)bytes); cc_emit((uint8_t)(bytes>>8));
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode)cc_units_note(UR_PC,cc.emitted,CC_CODE_BASE+pc);
#endif
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
        encoded=cc_reloc_get(n*RELOCATION_BYTES)|((uint16_t)cc_reloc_get(n*RELOCATION_BYTES+1)<<8);
        if((encoded&0x7fffu)+CC_CODE_BASE==entry+1) {
            coefficient=cc_reloc_get(n*RELOCATION_BYTES+2)|((uint16_t)cc_reloc_get(n*RELOCATION_BYTES+3)<<8);
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
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode)cc_units_note(UR_PC,cc.emitted+1,cc.emitted);
#endif
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
void cc_load_indirect(TypeId type) FRONTEND {
    if(cc_family(type)==K_FILE) { cc_fail(54); return; }
    if(cc_family(type)==K_SET) { cc_set_load(type); return; }
    if(cc_family(type)==K_STRING||cc_family(type)==K_REAL||cc_aggregate(type)) return;
    if(type==TYPE_NONE) { cc_fail(44); return; }
    if(CCT(type,width)==1) { cc_emit(0x6e); cc_emit(0x26); cc_emit(0); }
    else { cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb); }
}
void cc_store_indirect(TypeId type) FRONTEND {
    cc_emit(0x7d); cc_emit(0x12);
    if(CCT(type,width)==2) { cc_emit(0x13); cc_emit(0x7c); cc_emit(0x12); }
}
void cc_call(uint16_t address) FRONTEND {
    if((address>=RT_ALLOC&&address<=RT_STACKSET)||
       (address>=RT_FORMAT&&address<=RT_INPUT_END))cc_require_full_memory();
    cc_guard(); cc_op16(0xcd,address);
}
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
#ifdef ZP_UNITS_EXPERIMENT
    cc.sources->unit_qualifier=0;
#endif
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

int16_t cc_lookup_decl(void) FRONTEND {
    Symbol symbol;Source view;uint16_t loaded=0xffffu,base=0;
    uint16_t i; int16_t field; uint8_t j;uint16_t scope=cc.scope;uint8_t n=(uint8_t)strlen(cc.name);
    for(j=cc.with_depth;j;--j) {
        field=cc_field_lookup(cc.with_types[j-1]);
        if(field>=0) return field;
    }
    for(;;) {
        i=cc.symbol_count;
        while(i--) {
            if(!(i&63u)&&!cc_poll())return -1;
            if(!cc_symbol_read(i,&symbol))return -1;
            if(symbol.role>=S_FIELD||symbol.owner!=scope)continue;
#ifdef ZP_UNITS_EXPERIMENT
            if((symbol.role==S_PROCEDURE||symbol.role==S_FUNCTION)&&
               (cc_rget(symbol.initial_type,R_FLAGS)&RF_UNIT_IMPORT))continue;
#endif
            if(symbol.role==S_STANDARD) {
                if(cc_is(standard_names[symbol.initial_type-1])) {
                    CCS_SET(i,type,text_file_type()); return (int16_t)i;
                }
                continue;
            }
            if(symbol.role==S_HOOK) {
                if(cc_is(hook_names[symbol.initial_type])) {
#ifndef ZP_UNITS_EXPERIMENT
                    if(!cc.suppress)cc.console_dynamic=1;
#endif
                    return (int16_t)i;
                }
                continue;
            }
            if(symbol.role==S_SYSTEM) {
                for(j=0;j<sizeof(system_variables)/sizeof(system_variables[0]);++j)
                    if(symbol.address==system_variables[j].address&&cc_is(system_variables[j].name)) {
#ifndef ZP_UNITS_EXPERIMENT
                        if(!cc.suppress&&symbol.address==RT_CBREAK)cc.console_dynamic=1;
#endif
                        return (int16_t)i;
                    }
                continue;
            }
            if(symbol.name_length!=n) continue;
            if(name_matches_view(symbol.name_start,n,&view,&loaded,&base)) {
#ifndef ZP_UNITS_EXPERIMENT
                if(!cc.suppress&&symbol.role==S_ADDRESS)cc.console_dynamic=1;
#endif
                return (int16_t)i;
            }
        }
        if(!scope) break;
        scope=cc_rget(scope,R_PARENT);
    }
    return -1;
}
#ifdef ZP_UNITS_EXPERIMENT
int16_t cc_lookup(void) FRONTEND {
    int16_t id;
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_qualifier)return cc_units_lookup();
#endif
    id=cc_lookup_decl();
    /* Declaring a shadow must not count as using the outer runtime symbol.
     * Only expression/statement lookup commits runtime dependencies. */
    if(id>=0&&!cc.suppress) {
        uint8_t role=CCS(id,role);
        if(role==S_SYSTEM&&
           (CCS(id,address)==RT_HEAPPTR||CCS(id,address)==RT_RECURPTR))cc_require_full_memory();
        if(role==S_HOOK||role==S_ADDRESS||
           (role==S_SYSTEM&&(cc.sources->unit_mode||CCS(id,address)==RT_CBREAK)))
            cc.console_dynamic=1;
    }
#ifdef ZP_UNITS_EXPERIMENT
    if(id<0&&cc.sources->unit_page!=255&&!cc.sources->unit_mode)return cc_units_lookup();
#endif
    return id;
}
static void storage_address(uint8_t opcode,uint16_t id) {
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode&&CCS(id,address)>=GLOBAL_BASE&&CCS(id,role)!=S_ADDRESS) {
        cc_units_global(opcode,CCS(id,address));return;
    }
#endif
    cc_op16(opcode,CCS(id,address));
}
#else
#define storage_address(opcode,id) cc_op16((opcode),CCS((id),address))
#endif
static int16_t find_field(TypeId type,uint16_t field) {
    uint16_t i,variant;
    uint8_t n=(uint8_t)strlen(cc.name);
    int16_t found;
    for(i=0;i<cc.symbol_count;++i) if(CCS(i,role)==S_FIELD&&CCS(i,owner)==type) {
        if(field!=0xffffu) { if(field!=i) continue; }
        else {
            if(CCS(i,name_length)!=n) continue;
            if(!name_matches(CCS(i,name_start),n)) continue;
        }
        cc.field_offset=CCS(i,address); return (int16_t)i;
    }
    if(CCT(type,low)<0) return -1;
    variant=(uint16_t)CCT(type,low);
    for(i=0;i<cc.symbol_count;++i) if(CCS(i,role)==S_VLABEL&&CCS(i,owner)==type) {
        found=find_field(CCS(i,type),field);
        if(found>=0) { cc.field_offset+=CCS(variant,address); return found; }
    }
    return -1;
}
int16_t cc_field_lookup(TypeId type) FRONTEND { return find_field(type,0xffffu); }
uint16_t cc_field_path(TypeId type,uint16_t field) FRONTEND {
    return find_field(type,field)<0?0xffffu:cc.field_offset;
}
static void previous_with(void) {
    cc_emit(0x23); cc_emit(0x23); cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
}
void cc_with_address(uint16_t field) FRONTEND {
    uint8_t i=cc.with_depth;uint8_t distance=0;
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
uint8_t cc_frame_required(uint16_t scope) FRONTEND {
    while(scope) {
        if(cc_rget(scope,R_FLAGS)&RF_RECURSIVE) return 1;
        scope=cc_rget(scope,R_PARENT);
    }
    return 0;
}
void cc_frame(uint16_t scope) FRONTEND {
    uint16_t current=cc.scope;
    if(!scope||(scope!=cc.scope&&!cc_frame_required(scope))) { cc_op16(0x21,0); return; }
    cc_guard(); cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xe1); /* HL = IX */
    while(current!=scope&&!cc.error->code) {
        if(!current) { cc_fail(44); return; }
        cc_emit(0x2b); cc_emit(0x2b); cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
        current=cc_rget(current,R_PARENT);
    }
}
void cc_bind_storage(uint16_t id) FRONTEND {
    if(CCS(id,role)==S_HOOK) cc_hook_variable(id);
    if(CCS(id,role)==S_STANDARD&&!CCS(id,address)&&!cc.suppress&&!cc.error->code) {
        if((uint32_t)GLOBAL_BASE+cc.image->globals_bytes+176>=CC_STATIC_LIMIT) { cc_fail(98); return; }
        CCS_SET(id,type,text_file_type());
        CCS_SET(id,address,GLOBAL_BASE+cc.image->globals_bytes); cc.image->globals_bytes+=176;
    }
}
void cc_slot(uint16_t id) FRONTEND {
    cc_bind_storage(id);
    if(CCS(id,role)==S_ALIAS) cc_symbol_address(CCS(id,address));
    else if(CCS(id,role)==S_ADDRESS) cc_op16(0x21,CCS(id,address));
    else if(CCS(id,role)==S_TYPED) cc_address(0x21,CCS(id,address));
    else storage_address(0x21,id);
}
TypeId cc_standard_reference(uint8_t identity) FRONTEND {
    uint16_t i;
    for(i=0;i<cc.symbol_count;++i) if(CCS(i,role)==S_STANDARD&&CCS(i,initial_type)==identity) {
        CCS_SET(i,type,text_file_type()); cc_slot(i); return CCS(i,type);
    }
    cc_fail(99); return TYPE_NONE;
}
void cc_symbol_address(uint16_t id) FRONTEND {
    if(CCS(id,role)==S_FIELD) { cc_with_address(id); return; }
    cc_slot(id);
    if(CCS(id,role)==S_REFERENCE) { cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb); }
}
static uint8_t indirect_symbol(uint16_t id) {
    return CCS(id,role)==S_ALIAS||CCS(id,role)==S_FIELD||CCS(id,role)==S_REFERENCE;
}
void cc_destination(uint16_t id) FRONTEND {
    if(CCS(id,role)==S_HOOK) cc_hook_variable(id);
    if(indirect_symbol(id)) {
        cc_guard(); cc_emit(0xe5); cc_symbol_address(id); cc_emit(0xeb); cc_emit(0xe1);
    } else if(CCS(id,role)==S_TYPED) cc_address(0x11,CCS(id,address));
    else storage_address(0x11,id);
}
void cc_load_symbol(uint16_t id) FRONTEND {
    if(CCS(id,role)==S_HOOK) cc_hook_variable(id);
    if(CCS(id,type)==TYPE_NONE) { cc_fail(44); return; }
    if(cc_family(CCS(id,type))==K_FILE) { cc_fail(54); return; }
    if(cc_family(CCS(id,type))==K_SET) { cc_symbol_address(id); cc_set_load(CCS(id,type)); return; }
    if(cc_family(CCS(id,type))==K_STRING||cc_family(CCS(id,type))==K_REAL||cc_aggregate(CCS(id,type))) cc_symbol_address(id);
    else if(indirect_symbol(id)) {
        cc_symbol_address(id);
        cc_load_indirect(CCS(id,type));
    }
    else if(CCT(CCS(id,type),width)==1) {
        if(CCS(id,role)==S_TYPED) cc_address(0x3a,CCS(id,address)); else storage_address(0x3a,id);
        cc_emit(0x6f); cc_emit(0x26); cc_emit(0);
    } else if(CCS(id,role)==S_TYPED) cc_address(0x2a,CCS(id,address)); else storage_address(0x2a,id);
}
void cc_store_symbol(uint16_t id) FRONTEND {
    uint8_t op;
    if(CCS(id,role)==S_HOOK) cc_hook_variable(id);
    if(CCS(id,type)==TYPE_NONE) { cc_fail(44); return; }
    op=CCT(CCS(id,type),width)==1?0x32:0x22;
    if(indirect_symbol(id)) {
        cc_guard(); cc_emit(0xe5); cc_symbol_address(id); cc_emit(0xd1); cc_emit(0x73);
        if(op==0x22) { cc_emit(0x23); cc_emit(0x72); }
        return;
    }
    if(op==0x32) cc_emit(0x7d);
    if(CCS(id,role)==S_TYPED) cc_address(op,CCS(id,address)); else storage_address(op,id);
}
void cc_check_range(TypeId type) FRONTEND {
    if(type==TYPE_NONE) { cc_fail(44); return; }
    if(!cc.range_check||!cc_ordinal(type)) return;
    cc_op16(0x11,(uint16_t)CCT(type,low)); cc_op16(0x01,(uint16_t)CCT(type,high)); cc_call(RT_RANGE);
}
void cc_result_symbol(uint16_t routine) FRONTEND {
    uint16_t i; uint8_t j;SourcePosition left,right;
    for(i=0;i<cc.symbol_count;++i) if(CCS(i,role)<S_FIELD&&CCS(i,owner)==cc.scope&&CCS(i,name_length)==CCS(routine,name_length)) {
        j=0;
        if(CCS(routine,name_length)) {
            left=cc_reference_position(CCS(i,name_start));right=cc_reference_position(CCS(routine,name_start));
            for(;j<CCS(routine,name_length);++j)if(name_byte(left,j)!=name_byte(right,j)) break;
        }
        if(j==CCS(routine,name_length)) { cc_fail(43); return; }
    }
    if(cc.symbol_count==SYMBOL_LIMIT) { cc_fail(99); return; }
    i=cc.symbol_count++; cc_symbol_copy(i,routine);
    CCS_SET(i,owner,cc.scope); CCS_SET(i,role,S_RESULT);
    cc_storage(i,CCT(CCS(routine,type),width));
}
void cc_convert(TypeId target,TypeId source) FRONTEND {
    if(cc_aggregate(target)) { if(target!=source) cc_fail(44); return; }
    if(target==TYPE_NONE||source==TYPE_NONE||!cc_compatible(target,source)) { cc_fail(44); return; }
    source=cc_text_value(source);
    if(cc_family(target)==K_REAL&&cc_family(source)==K_INTEGER) cc_real_unary(ZR_FROM_INT,source,cc.temporaries);
    if(cc_family(target)==K_CHAR&&cc_family(source)==K_STRING) cc_call(RT_STRINGCHAR);
    cc_check_range(target);
}
void cc_set_load(TypeId type) FRONTEND {
    cc_temporary(32);
    cc_emit(0x3e); cc_emit((uint8_t)(CCT(CCT(type,element),low)>>3));
    cc_emit(0x0e); cc_emit((uint8_t)CCT(type,width)); cc_call(RT_SETLOAD);
}
void cc_set_store(TypeId type) FRONTEND {
    cc_emit(0x3e); cc_emit((uint8_t)(CCT(CCT(type,element),low)>>3));
    cc_emit(0x0e); cc_emit((uint8_t)CCT(type,width)); cc_call(RT_SETSTORE);
}
TypeId cc_text_value(TypeId type) FRONTEND {
    if(!cc_char_array(type)) return type;
    cc_temporary(CCT(type,width)+1); cc_emit(0x3e); cc_emit((uint8_t)CCT(type,width));
    cc_emit(0x12); cc_emit(0x13); cc_op16(0x01,CCT(type,width)); cc_call(RT_MOVE);
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
#ifdef ZP_UNITS_EXPERIMENT
    cc.sources->unit_qualifier=cursor.unit_qualifier;
#endif
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
    if(id>=0&&CCS(id,role)==S_TYPE) return CCS(id,type);
    return id<0?cc_predefined_type():-1;
}
/* Native color names are ordinary predefined Integer constants. Callers
 * resolve user declarations first, exactly as for MaxInt and Pi. */
int16_t cc_predefined_color(void) FRONTEND {
    static const char *const names[]={"black","blue","green","cyan","red","magenta","brown","lightgray",
        "darkgray","lightblue","lightgreen","lightcyan","lightred","lightmagenta","yellow","white"};
    uint8_t i;
    for(i=0;i<16;++i)if(cc_is(names[i]))return i;
    return -1;
}
void cc_constant(TypeId *type,uint16_t *value) FRONTEND {
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
        if(id>=0&&CCS(id,role)==S_CONSTANT) { *type=CCS(id,type); *value=CCS(id,address); }
        else if(id<0&&(cc_is("true")||cc_is("false"))) { *type=K_BOOLEAN; *value=cc_is("true"); }
        else if(id<0&&cc_is("maxint")) *value=32767;
        else if(id<0&&cc_is("pi")) { *type=K_REAL; *value=REAL_PI; }
        else if(id<0&&(id=cc_predefined_color())>=0) *value=(uint16_t)id;
        else cc_fail(41);
    } else cc_fail(25);
    if(sign&&!cc_numeric(*type)) cc_fail(25);
    if(sign==2) *value=cc_family(*type)==K_REAL?*value^0x8000u:(uint16_t)(0u-*value);
    cc_next();
}
