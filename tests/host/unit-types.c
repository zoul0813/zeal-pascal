/* Independent type-graph fixtures exercise the production decoder. Native
 * author/import/run tests separately qualify parsing, serialization and code. */
#include "compiler_units.h"
#include <assert.h>
/* Keep the artifact type-envelope validator in this translation unit so the
 * fixtures exercise its production bounds checks before semantic import. */
#include "../../src/compiler_units_object.c"
#include <stdio.h>

Compiler cc;
uint8_t sources_byte(const Sources *sources,uint8_t file,uint16_t at){(void)sources;(void)file;(void)at;assert(!"unexpected ordinary source read");return 0;}
static Diagnostic diagnostic;
static Type types[256];static Symbol symbols[1024];
static uint8_t arena[PAGE_SIZE],objects[2][PAGE_SIZE];
static uint16_t cursors[2];
static CompilerSources source_state;
static uint8_t reference_data[1024][10];
static unsigned reference_reads;
static uint8_t stack_ok;
static unsigned checks;static int cancel_after;
#define A 256u
#define B 280u
static void check(int ok){++checks;if(!ok)fprintf(stderr,"type check %u failed, diagnostic %u\n",checks,diagnostic.code);assert(ok); }
static uint16_t get(const uint8_t *p,uint16_t at){return p[at]|((uint16_t)p[at+1]<<8);}
static void put(uint8_t *p,uint16_t at,uint16_t v){p[at]=(uint8_t)v;p[at+1]=(uint8_t)(v>>8);}
static uint8_t *object(uint16_t module){assert(module==A||module==B);return objects[module==B];}
uint16_t cc_unit_word(uint16_t at){assert(at+1<PAGE_SIZE);return get(arena,at);}
void cc_unit_put(uint16_t at,uint16_t v){assert(at+1<PAGE_SIZE);put(arena,at,v);}
uint8_t cc_unit_module_byte(uint16_t module,uint16_t at){assert(at<PAGE_SIZE);return object(module)[at];}
uint16_t cc_unit_module_word(uint16_t module,uint16_t at){assert(at+1<PAGE_SIZE);return get(object(module),at);}
uint16_t cc_unit_record(uint16_t kind,uint16_t a,uint16_t b,uint16_t c,uint16_t d,uint16_t e){
    uint16_t at=cc_unit_word(UH_USED),words[]={kind,a,b,c,d,e};uint8_t i;
    if(diagnostic.code)return 0;assert(at+12<=PAGE_SIZE);
    for(i=0;i<6;++i)cc_unit_put(at+2*i,words[i]);cc_unit_put(UH_USED,at+12);return at;
}
uint16_t cc_unit_target(uint16_t module,uint16_t wire){assert(module==A&&wire==2);return B;}
uint8_t cc_stack_check(void){if(!stack_ok)cc_fail(99);return stack_ok;}
uint8_t cc_poll(void){if(cancel_after==0){cc_fail(COMPILE_INTERRUPTED);return 0;}if(cancel_after>0)--cancel_after;return 1;}
void cc_fail(uint8_t code){if(!diagnostic.code)diagnostic.code=code;}
static uint16_t field_get(const uint8_t *p,uint8_t field){uint8_t at=field>>1;return field&1?get(p,at):p[at];}
static void field_put(uint8_t *p,uint8_t field,uint16_t v){uint8_t at=field>>1;if(field&1)put(p,at,v);else p[at]=(uint8_t)v;}
uint16_t cc_type_value(uint16_t id,uint8_t field){assert(id<256);return field_get((const uint8_t*)(types+id),field);}
void cc_type_set(uint16_t id,uint8_t field,uint16_t value){assert(id<256);field_put((uint8_t*)(types+id),field,value);}
uint16_t cc_symbol_value(uint16_t id,uint8_t field){assert(id<1024);return field_get((const uint8_t*)(symbols+id),field);}
void cc_symbol_set(uint16_t id,uint8_t field,uint16_t value){assert(id<1024);field_put((uint8_t*)(symbols+id),field,value);}
void cc_symbol_clear(uint16_t id){assert(id<1024);memset(symbols+id,0,sizeof(Symbol));}
TypeId cc_new_type(TypeId family,uint16_t width,int16_t low,int16_t high,TypeId capacity){
    uint16_t id=cc.type_count++;assert(id<256);types[id].family=family;types[id].width=width;
    types[id].low=low;types[id].high=high;types[id].capacity=capacity;return id;
}
TypeId cc_family(TypeId type){return type<cc.type_count?types[type].family:TYPE_NONE;}
uint8_t cc_ordinal(TypeId type){return type<cc.type_count&&((cc_family(type)<K_ARRAY&&cc_family(type)!=K_STRING&&cc_family(type)!=K_REAL)||cc_family(type)==K_WORD);}
uint8_t cc_aggregate(TypeId type){return cc_family(type)==K_ARRAY||cc_family(type)==K_RECORD;}
uint8_t cc_reference_read(uint16_t id,uint8_t *out,uint8_t count){assert(id<1024&&count<=10);++reference_reads;memcpy(out,reference_data[id],count);return 1;}
void cc_reference_put(uint16_t id,uint8_t field,uint8_t value){assert(id<1024&&field<10);reference_data[id][field]=value;}
/* Routine paths are not used by these type-only fixtures. */
void cc_routine_clear(uint16_t id){(void)id;}
void cc_rput(uint16_t scope,uint8_t field,uint16_t value){(void)scope;(void)field;(void)value;}
static void reset(void){
    memset(&cc,0,sizeof(cc));memset(&diagnostic,0,sizeof(diagnostic));memset(types,0,sizeof(types));
    memset(symbols,0,sizeof(symbols));memset(arena,0,sizeof(arena));memset(objects,0,sizeof(objects));memset(&source_state,0,sizeof(source_state));memset(reference_data,0,sizeof(reference_data));reference_reads=0;stack_ok=1;cancel_after=-1;
    cc.error=&diagnostic;cc.sources=&source_state;cc.type_count=6;cc_unit_put(UH_USED,304);cursors[0]=cursors[1]=48;
    types[0]=(Type){2,-32768,32767,K_INTEGER,0,0};types[1]=(Type){0,0,0,K_STRING,0,0};
    types[2]=(Type){1,0,1,K_BOOLEAN,0,0};types[3]=(Type){1,0,255,K_CHAR,0,0};
    types[4]=(Type){1,0,255,K_INTEGER,0,0};types[5]=(Type){6,0,0,K_REAL,0,0};
    put(objects[0],42,48);put(objects[1],42,48);
}
static uint16_t type(uint16_t module,uint16_t family,uint16_t width,int16_t low,int16_t high,
                     uint16_t capacity,uint16_t element,uint16_t origin,uint16_t wire,uint16_t members){
    uint8_t *p=object(module);uint16_t n=get(p,38),at=cursors[module==B],words[]={family,width,(uint16_t)low,(uint16_t)high,capacity,element,origin,wire,members};uint8_t i;
    put(p,38,n+1);for(i=0;i<9;++i)put(p,at+2*i,words[i]);cursors[module==B]+=18;return n+6;
}
static void member(uint16_t module,uint8_t role,uint16_t field_type,uint16_t offset,uint16_t tag,const char *name){
    uint8_t *p=object(module);uint16_t at=cursors[module==B],n=(uint16_t)strlen(name);
    p[at]=role;p[at+1]=(uint8_t)n;put(p,at+2,field_type);put(p,at+4,offset);put(p,at+6,tag);
    memcpy(p+at+10,name,n);cursors[module==B]+=10+n;
}
static uint16_t actual(uint16_t module,uint16_t wire){
    uint16_t at;for(at=304;at<cc_unit_word(UH_USED);at+=12)
        if(cc_unit_word(at)==UR_TYPE&&cc_unit_word(at+2)==module&&cc_unit_word(at+4)==wire)return cc_unit_word(at+6);
    assert(0);return 0;
}
static uint16_t export(uint16_t module,uint8_t role,uint16_t result,uint16_t value,uint16_t params){
    uint8_t *p=object(module);uint16_t at=cursors[module==B];put(p,20,1);cc_unit_put(module+10,at);
    p[at+1]=role;put(p,at+4,result);put(p,at+6,params);put(p,at+10,value);return at;
}
static void pointers(void){
    uint16_t p,r,n;
    reset();type(A,K_POINTER,2,0,0,0,7,0,0,0);type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,6,0,65535,"next");
    check(cc_unit_types_ready(A));p=actual(A,6);r=actual(A,7);
    check(types[p].element==r&&symbols[0].type==p&&types[r].width==2);
    n=cc.type_count;check(cc_unit_types_ready(A)&&cc.type_count==n&&cc.symbol_count==1);
    check((cc_reference_position(symbols[0].name_start)>>16)==(0x8000u|A));
    reset();type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,7,0,65535,"next");type(A,K_POINTER,2,0,0,0,6,0,0,0);
    check(cc_unit_types_ready(A)&&types[actual(A,7)].element==actual(A,6));
    reset();type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,6,0,65535,"bad");
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();type(A,K_ARRAY,2,0,0,K_BOOLEAN,6,0,0,0);
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
}
static void variants(void){
    uint16_t r;
    reset();cc.symbol_count=13;
    type(A,K_RECORD,7,1,0,0,0,0,0,4);member(A,S_FIELD,K_BOOLEAN,0,65535,"tag");member(A,S_VARIANT,K_BOOLEAN,1,0,"");
    member(A,S_VLABEL,7,0,65535,"");member(A,S_VLABEL,8,1,65535,"");
    type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,K_INTEGER,0,65535,"i");
    type(A,K_RECORD,6,-1,0,0,0,0,0,1);member(A,S_FIELD,K_REAL,0,65535,"r");
    check(cc_unit_types_ready(A));r=actual(A,6);
    check(types[r].low==14&&symbols[14].name_start==13&&symbols[14].address==1);
    check(types[symbols[15].type].width==2&&types[symbols[16].type].width==6);
    reset();type(A,K_RECORD,1,-1,0,0,0,0,0,2);member(A,S_FIELD,K_BYTE,0,65535,"x");member(A,S_FIELD,K_BYTE,1,65535,"y");
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
}
static void descriptive_variant_labels(void){
    reset();type(A,K_RECORD,2,0,0,0,0,0,0,2);member(A,S_VARIANT,K_BYTE,0,65535,"");member(A,S_VLABEL,7,300,65535,"");
    type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,K_INTEGER,0,65535,"a");check(cc_unit_types_ready(A));
    reset();type(A,K_INTEGER,1,1,3,0,0,0,0,0);
    type(A,K_RECORD,2,0,0,0,0,0,0,2);member(A,S_VARIANT,6,0,65535,"");member(A,S_VLABEL,8,5,65535,"");
    type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,K_INTEGER,0,65535,"b");check(cc_unit_types_ready(A));
}
static void identities(void){
    uint16_t at,id;
    reset();type(B,6,1,0,1,0,0,0,0,0);
    type(A,0,0,0,0,0,0,2,6,0);type(A,0,0,0,0,0,0,2,6,0);
    check(cc_unit_types_ready(A)&&cc_unit_types_ready(B));
    check(actual(A,6)==actual(A,7)&&actual(A,6)==actual(B,6)&&cc.type_count==7);
    at=export(A,3,6,0,0);id=(uint16_t)cc_unit_import_symbol(A,at);
    check(symbols[id].role==S_TYPE&&symbols[id].type==actual(B,6)&&symbols[id].name_length==0);
    reset();type(B,6,1,0,1,0,0,0,0,0);type(A,0,0,0,0,0,0,2,7,0);
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();type(A,6,1,0,1,0,0,0,0,0);type(A,7,1,0,1,0,0,0,0,0);
    check(cc_unit_types_ready(A)&&types[actual(A,6)].family!=types[actual(A,7)].family);
    export(A,2,6,2,0);check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
}
static void bounds(void){
    uint16_t at;
    reset();type(A,K_INTEGER,1,1,3,0,0,0,0,0);type(A,K_ARRAY,6,0,0,6,K_INTEGER,0,0,0);
    check(cc_unit_types_ready(A)&&types[actual(A,7)].capacity==actual(A,6));
    reset();type(A,K_INTEGER,1,1,3,0,0,0,0,0);type(A,K_ARRAY,5,0,0,6,K_INTEGER,0,0,0);
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();type(A,6,1,0,1,0,0,0,0,0);type(A,6,1,1,2,0,0,0,0,0);
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();type(A,K_FILE,176,0,0,FILE_TYPED,K_INTEGER,0,0,0);check(cc_unit_types_ready(A));
    at=export(A,0,TYPE_NONE,0,1);put(object(A),at+16,6);
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();export(A,2,K_INTEGER,65535,0);check(cc_unit_types_ready(A));
    reset();export(A,2,K_BOOLEAN,2,0);check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
}
static void envelopes(void){
    uint16_t cursor;
    reset();type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,K_INTEGER,0,65535,"x");
    cursor=48;check(type_records(A,&cursor,cursors[0])&&cursor==cursors[0]);
    cursor=48;check(!type_records(A,&cursor,65));
    put(object(A),48+18+2,TYPE_NONE);cursor=48;check(!type_records(A,&cursor,cursors[0]));
    put(object(A),48+18+2,K_INTEGER);put(object(A),48+18+6,0);cursor=48;
    check(!type_records(A,&cursor,cursors[0]));
    reset();type(A,6,2,0,1,0,0,0,0,0);cursor=48;check(!type_records(A,&cursor,cursors[0]));
    reset();type(A,K_POINTER,2,1,0,0,K_INTEGER,0,0,0);cursor=48;check(!type_records(A,&cursor,cursors[0]));
    reset();type(A,0,0,0,0,0,0,1,6,0);cursor=48;check(!type_records(A,&cursor,cursors[0]));
    reset();put(object(A),36,1);type(A,0,0,0,0,0,0,2,6,0);cursor=48;check(type_records(A,&cursor,cursors[0]));
    put(object(A),50,2);cursor=48;check(!type_records(A,&cursor,cursors[0]));
    reset();type(A,K_FILE,176,0,0,FILE_TYPED,TYPE_NONE,0,0,0);cursor=48;check(!type_records(A,&cursor,cursors[0]));
    reset();put(object(A),38,TYPE_LIMIT);cursor=48;check(!type_records(A,&cursor,100));
}
static void cancellations(void){
    uint16_t cursor;
    reset();type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,K_INTEGER,0,65535,"x");cancel_after=0;
    check(!cc_unit_types_ready(A)&&diagnostic.code==COMPILE_INTERRUPTED);
    reset();export(A,2,K_INTEGER,42,0);cancel_after=0;check(!cc_unit_types_ready(A)&&diagnostic.code==COMPILE_INTERRUPTED);
    reset();type(A,6,1,0,1,0,0,0,0,0);cursor=48;cancel_after=0;
    check(!type_records(A,&cursor,cursors[0])&&diagnostic.code==COMPILE_INTERRUPTED&&cursor==48);
    reset();cc_new_type(6,1,0,1,0);cancel_after=0;
    check(cc_unit_types_bytes()==0&&diagnostic.code==COMPILE_INTERRUPTED);
}
static void deferred_pointer_paths(void){
    reset();
    type(A,K_POINTER,2,0,0,0,8,0,0,0);type(A,K_POINTER,2,0,0,0,9,0,0,0);
    type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,7,0,65535,"owner");
    type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,8,0,65535,"hdr");
    check(cc_unit_types_ready(A));
    check(types[actual(A,6)].element==actual(A,8)&&types[actual(A,7)].element==actual(A,9));
    reset();
    type(A,K_POINTER,2,0,0,0,7,0,0,0);
    type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,8,0,65535,"a");
    type(A,K_RECORD,2,-1,0,0,0,0,0,1);member(A,S_FIELD,7,0,65535,"b");
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();type(B,K_POINTER,2,0,0,0,7,0,0,0);type(B,K_RECORD,2,-1,0,0,0,0,0,1);member(B,S_FIELD,6,0,65535,"next");
    type(A,0,0,0,0,0,0,2,6,0);type(A,0,0,0,0,0,0,2,7,0);
    check(cc_unit_types_ready(A)&&types[actual(A,6)].element==actual(A,7));
    check(actual(A,6)==actual(B,6)&&symbols[0].type==actual(B,6));
    reset();type(A,6,1,0,1,0,0,0,0,0);stack_ok=0;
    check(!cc_unit_types_ready(A)&&diagnostic.code==99);
}
static void source_reference_order(void){
    uint16_t raw,a,b,c,replay;
    reset();raw=cc_source_reference(0x81000040UL);check(cc.sources->highest_reference==0);
    reference_reads=0;a=cc_source_reference(10);b=cc_source_reference(20);c=cc_source_reference(30);
    check(reference_reads==0&&a!=b&&b!=c&&cc.sources->highest_reference==30);
    replay=cc_source_reference(20);check(replay==b&&reference_reads>0);
    cc.range_check=1;check(cc_source_reference(20)!=b);
    reference_reads=0;check(cc_source_reference(0x81000040UL)!=raw&&reference_reads==0&&cc.sources->highest_reference==30);
}
static void unsigned_types(void){
    uint16_t cursor,at,id;
    reset();type(A,K_WORD,2,0,-1,0,0,0,0,0);type(B,K_WORD,2,0,-1,0,0,0,0,0);
    check(cc_unit_types_ready(A)&&cc_unit_types_ready(B)&&actual(A,6)==actual(B,6)&&actual(A,6)==cc.word_type);
    export(A,2,6,65535,0);check(cc_unit_types_ready(A));
    reset();type(A,K_LONGWORD,4,0,-1,0,0,0,0,0);type(B,K_LONGWORD,4,0,-1,0,0,0,0,0);
    check(cc_unit_types_ready(A)&&cc_unit_types_ready(B)&&actual(A,6)==actual(B,6)&&actual(A,6)==cc.longword_type);
    at=export(A,2,6,0,0);put(object(A),at+12,4);put(object(A),at+14,0x3926);put(object(A),at+16,0xcbf4);
    check(cc_unit_types_ready(A));id=(uint16_t)cc_unit_import_symbol(A,at);
    check(symbols[id].role==S_CONSTANT&&cc_unsigned_literal(symbols[id].address)==0xcbf43926UL);
    reset();type(A,K_WORD,2,32760,-32761,0,0,0,0,0);type(A,K_ARRAY,16,0,0,6,K_BYTE,0,0,0);
    check(cc_unit_types_ready(A));
    reset();type(A,K_WORD,2,32760,-32761,0,0,0,0,0);type(A,K_ARRAY,15,0,0,6,K_BYTE,0,0,0);
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();type(A,K_WORD,4,0,-1,0,0,0,0,0);cursor=48;
    check(!type_records(A,&cursor,cursors[0])&&!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();type(A,K_LONGWORD,2,0,-1,0,0,0,0,0);cursor=48;
    check(!type_records(A,&cursor,cursors[0])&&!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();type(A,K_LONGWORD,4,0,0,0,0,0,0,0);check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
    reset();type(A,K_LONGWORD,4,0,-1,0,0,0,0,0);export(A,2,6,0,0);
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
}
static void unsigned_set_bounds(void){
    reset();type(A,K_WORD,2,0,-1,0,0,0,0,0);type(A,K_SET,0,0,0,0,6,0,0,0);
    check(!cc_unit_types_ready(A)&&diagnostic.code==UNIT_FORMAT);
}
int main(void){unsigned_set_bounds();pointers();variants();descriptive_variant_labels();identities();bounds();envelopes();cancellations();deferred_pointer_paths();source_reference_order();unsigned_types();printf("%u unit type assertions passed\n",checks);return 0;}
