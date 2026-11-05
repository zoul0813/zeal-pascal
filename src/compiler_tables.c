#include "compiler.h"

/* Compiler records share one native page arena. Each stream's128-byte first
 * segment doubles to8192, then grows by16KiB segments. Logical offsets select
 * their segment directly; this directory holds addresses, never record data.
 * Five seven-segment prefixes plus32full-page segments bound every layout. */
#define SEGMENT_LIMIT 67u
#define TABLE_SEGMENTS 39u
#define PREFIX_LENGTH 16256u
enum { TABLE_SYMBOL,TABLE_TYPE,TABLE_ROUTINE,TABLE_RELOCATION,TABLE_REFERENCE,TABLE_COUNT };
typedef struct {uint32_t length;uint8_t count,ids[TABLE_SEGMENTS];} Table;
static uint32_t segments[SEGMENT_LIMIT];
static Table tables[TABLE_COUNT];
static uint8_t pages[32],page_count,segment_count,started;
static uint32_t lives[32],arena_length;
#ifndef __SDCC
static uintptr_t stack_top;
#endif
static void initialize(void) {
    uint8_t i;
    if(started)return;
    for(i=0;i<TABLE_COUNT;++i){tables[i].length=0;tables[i].count=0;}
    started=1;
}
static uint8_t reserve(uint32_t end) {
    uint8_t p;
    if(end>32UL*PAGE_SIZE){cc_fail(99);return 0;}
    while((uint32_t)page_count*PAGE_SIZE<end) {
        p=page_new(PAGE_COMPILER);if(p==255){cc_fail(99);return 0;}
        pages[page_count]=p;lives[page_count]=page_lifetime(p);++page_count;
    }
    return 1;
}
static uint32_t locate(uint8_t table,uint32_t at,uint8_t writing,uint16_t *span) {
    Table *t;uint32_t rest;uint16_t word,length,relative;uint8_t bucket=0,id;
    initialize();t=&tables[table];
    if(!writing&&at>=t->length)return 0xffffffffUL;
    if(at<PREFIX_LENGTH) {
        word=(uint16_t)at+128u;
        /* floor(log2(word))-7, with three bounded16-bit comparisons. */
        if(word>=2048u){bucket+=4;word>>=4;}
        if(word>=512u){bucket+=2;word>>=2;}
        if(word>=256u)++bucket;
        length=128u<<bucket;relative=(uint16_t)at-(length-128u);
    } else {
        rest=at-PREFIX_LENGTH;
        if(rest>=32UL*PAGE_SIZE){cc_fail(99);return 0xffffffffUL;}
        bucket=7u+(uint8_t)(rest>>14);length=PAGE_SIZE;relative=(uint16_t)(rest&0x3fffu);
    }
    while(t->count<=bucket) {
        if(!writing)return 0xffffffffUL;
        if(segment_count==SEGMENT_LIMIT){cc_fail(99);return 0xffffffffUL;}
        word=t->count<7u?128u<<t->count:PAGE_SIZE;
        if(!reserve(arena_length+word))return 0xffffffffUL;
        id=segment_count++;segments[id]=arena_length;t->ids[t->count++]=id;
        t->length+=word;arena_length+=word;
    }
    if(span)*span=length-relative;
    return segments[t->ids[bucket]]+relative;
}
static uint8_t byte_get(uint8_t table,uint32_t at) {
    uint32_t offset=locate(table,at,0,0);uint8_t index,p;
    if(offset==0xffffffffUL)return 0;
    index=(uint8_t)(offset/PAGE_SIZE);
    if(index>=page_count){cc_fail(99);return 0;}
    p=pages[index];
    if(page_role(p)!=PAGE_COMPILER||page_lifetime(p)!=lives[index]){cc_fail(99);return 0;}
    return far_get(p,(uint16_t)(offset%PAGE_SIZE));
}
static void byte_put(uint8_t table,uint32_t at,uint8_t value) {
    uint32_t offset=locate(table,at,1,0);uint8_t index,p;
    if(offset==0xffffffffUL)return;
    index=(uint8_t)(offset/PAGE_SIZE);
    if(index>=page_count){cc_fail(99);return;}
    p=pages[index];
    if(page_role(p)!=PAGE_COMPILER||page_lifetime(p)!=lives[index]){cc_fail(99);return;}
    far_set(p,(uint16_t)(offset%PAGE_SIZE),value);
}
/* Resolve/authenticate one contiguous span rather than repeating the arena
 * walk for every scalar byte. Caller storage is retained near RAM; records
 * crossing a segment or physical page are copied in separate bounded spans. */
static uint8_t read(uint8_t table,uint32_t at,uint8_t *out,uint16_t count) {
    uint32_t offset;uint16_t n,span,within;uint8_t index,p;
    while(count) {
        offset=locate(table,at,0,&span);
        if(offset==0xffffffffUL){memset(out,0,count);return !cc.error->code;}
        index=(uint8_t)(offset/PAGE_SIZE);within=(uint16_t)(offset%PAGE_SIZE);
        if(index>=page_count){cc_fail(99);return 0;}
        p=pages[index];
        if(page_role(p)!=PAGE_COMPILER||page_lifetime(p)!=lives[index]){cc_fail(99);return 0;}
        n=count;if(n>span)n=span;if(n>PAGE_SIZE-within)n=PAGE_SIZE-within;
        if(n>FAR_READ_LIMIT)n=FAR_READ_LIMIT;
        if(!far_read(p,within,out,n)){cc_fail(99);return 0;}
        at+=n;out+=n;count-=n;
    }
    return 1;
}
static uint16_t get(uint8_t table,uint32_t at,uint8_t wide) {
    uint8_t bytes[2]={0,0};
    if(!wide)return byte_get(table,at);
    read(table,at,bytes,2);return bytes[0]|((uint16_t)bytes[1]<<8);
}
static void put(uint8_t table,uint32_t at,uint8_t wide,uint16_t value) {
    byte_put(table,at,(uint8_t)value);
    if(wide)byte_put(table,at+1,(uint8_t)(value>>8));
}
uint8_t cc_symbol_read(uint16_t id,Symbol *out) FRONTEND {
    memset(out,0,sizeof(*out));return read(TABLE_SYMBOL,(uint32_t)id*sizeof(Symbol),(uint8_t*)out,sizeof(*out));
}
uint8_t cc_reference_read(uint16_t id,uint8_t *out,uint8_t count) FRONTEND {
    if(count>10){cc_fail(99);return 0;}
    return read(TABLE_REFERENCE,(uint32_t)id*10u,out,count);
}
uint16_t cc_symbol_value(uint16_t id,uint8_t field) FRONTEND {return get(TABLE_SYMBOL,(uint32_t)id*sizeof(Symbol)+(field>>1),field&1);}
void cc_symbol_set(uint16_t id,uint8_t field,uint16_t value) FRONTEND {put(TABLE_SYMBOL,(uint32_t)id*sizeof(Symbol)+(field>>1),field&1,value);}
uint16_t cc_type_value(uint16_t id,uint8_t field) FRONTEND {return get(TABLE_TYPE,(uint32_t)id*sizeof(Type)+(field>>1),field&1);}
void cc_type_set(uint16_t id,uint8_t field,uint16_t value) FRONTEND {put(TABLE_TYPE,(uint32_t)id*sizeof(Type)+(field>>1),field&1,value);}
void cc_symbol_clear(uint16_t id) FRONTEND {uint8_t i;for(i=0;i<sizeof(Symbol);++i)byte_put(TABLE_SYMBOL,(uint32_t)id*sizeof(Symbol)+i,0);}
void cc_symbol_copy(uint16_t to,uint16_t from) FRONTEND {uint8_t i;for(i=0;i<sizeof(Symbol);++i)byte_put(TABLE_SYMBOL,(uint32_t)to*sizeof(Symbol)+i,byte_get(TABLE_SYMBOL,(uint32_t)from*sizeof(Symbol)+i));}
uint16_t cc_routine_value(uint16_t id,uint8_t field) FRONTEND {return get(TABLE_ROUTINE,(uint32_t)(id-1u)*ROUTINE_BYTES+field,field!=R_FLAGS);}
void cc_routine_set(uint16_t id,uint8_t field,uint16_t value) FRONTEND {put(TABLE_ROUTINE,(uint32_t)(id-1u)*ROUTINE_BYTES+field,field!=R_FLAGS,value);}
void cc_routine_clear(uint16_t id) FRONTEND {uint8_t i;for(i=0;i<ROUTINE_BYTES;++i)byte_put(TABLE_ROUTINE,(uint32_t)(id-1u)*ROUTINE_BYTES+i,0);}
uint8_t cc_reloc_get(uint16_t offset) FRONTEND {return byte_get(TABLE_RELOCATION,offset);}
void cc_reloc_put(uint16_t offset,uint8_t value) FRONTEND {byte_put(TABLE_RELOCATION,offset,value);}
uint8_t cc_reference_get(uint16_t id,uint8_t field) FRONTEND {return byte_get(TABLE_REFERENCE,(uint32_t)id*10u+field);}
void cc_reference_put(uint16_t id,uint8_t field,uint8_t value) FRONTEND {byte_put(TABLE_REFERENCE,(uint32_t)id*10u+field,value);}
uint8_t cc_references_end(void) FRONTEND {
    /* References are dead after parsing; their shared pages remain owned until
     * the compiler releases all metadata. No other stream can alias them. */
    initialize();tables[TABLE_REFERENCE].length=0;
    tables[TABLE_REFERENCE].count=0;return 1;
}
uint8_t cc_tables_end(void) FRONTEND {
    uint8_t p;
    while(page_count){p=pages[page_count-1];if(page_lifetime(p)==lives[page_count-1]&&!page_drop(p))return 0;--page_count;}
    segment_count=0;arena_length=0;started=0;return 1;
}
uint8_t cc_tables_begin(void) FRONTEND {
#ifndef __SDCC
    stack_top=0;
#endif
    return cc_tables_end();
}

#ifdef __SDCC
/* The compiler's native stack shares page3 with retained C data. Do not impose
 * a source nesting count: stop before an actual C frame reaches that data. */
static uint16_t stack_room(void) __naked {
    __asm
        .globl s__HEAP
        ld hl,#0
        add hl,sp
        ld de,#s__HEAP
        or a
        sbc hl,de
        jr nc,zp_stack_room_ok
        ld hl,#0
zp_stack_room_ok:
        ex de,hl
        ret
    __endasm;
}
#endif
uint8_t cc_stack_check(void) FRONTEND {
#ifdef __SDCC
    if(stack_room()>=768u)return 1;
#else
    uint8_t marker;
    uintptr_t here=(uintptr_t)&marker;
    if(!stack_top)stack_top=here;
    if((here>stack_top?here-stack_top:stack_top-here)<524288u)return 1;
#endif
    cc_fail(99);return 0;
}

uint8_t cc_variable(int16_t id) FRONTEND {
    return id>=0&&(CCS(id,role)==S_VARIABLE||CCS(id,role)==S_TYPED||
           CCS(id,role)==S_VALUE||CCS(id,role)==S_REFERENCE||CCS(id,role)==S_RESULT||CCS(id,role)==S_FIELD||CCS(id,role)==S_SYSTEM||
           CCS(id,role)==S_ADDRESS||CCS(id,role)==S_ALIAS||CCS(id,role)==S_STANDARD||CCS(id,role)==S_HOOK);
}
uint16_t cc_rget(uint16_t scope,uint8_t field) FRONTEND {
    return cc_routine_value(scope,field);
}
void cc_rput(uint16_t scope,uint8_t field,uint16_t value) FRONTEND {
    cc_routine_set(scope,field,value);
}

uint16_t cc_new_symbol(uint8_t role) FRONTEND {
    uint16_t id=cc.symbol_count;
    int16_t old=cc_lookup_decl();
    if(cc.token!=T_ID) cc_fail(53);
    else if(old>=0&&CCS(old,owner)==cc.scope&&CCS(old,role)!=S_SYSTEM&&CCS(old,role)!=S_STANDARD&&CCS(old,role)!=S_HOOK) cc_fail(43);
    else if(id==SYMBOL_LIMIT) cc_fail(99);
    if(cc.error->code) return 0;
    cc_symbol_clear(id);
    CCS_SET(id,name_start,cc_source_reference(cc.token_start));CCS_SET(id,name_length,(uint8_t)strlen(cc.name));
    CCS_SET(id,role,role); CCS_SET(id,owner,cc.scope); ++cc.symbol_count;
    return id;
}
