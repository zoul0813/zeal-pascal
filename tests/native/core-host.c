#include "hash-evidence.h"
/* Host structural/failure tests for the exact C front end used on Zeal.
 * These are deliberately not counted as native execution tests. */
#include "compiler.h"
#include "compiler_job.h"
#include "stream-hash.h"
#include "source_move.h"
#include "image.h"
#include "transfer_asset.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t memory[64][PAGE_SIZE],owned[64];
static uint32_t lifetimes[64],next_lifetime;
volatile uint32_t page_versions[64];
uint8_t platform_error;
uint8_t compiler_owned[8],compiler_allocating;
uint8_t compiler_parent_owned[8];
static int fail_after=-1,fail_drop=-1;
static unsigned checks,allocation_calls;
static uint8_t reference_probe,reference_probe_page;
static unsigned long reference_reads;
static uint8_t capture_probe,capture_probe_page;
static unsigned long capture_reads;
static uint8_t lexer_probe,lexer_probe_page,lexer_poll_probe;
static unsigned lexer_reads[256];
static uint16_t lexer_last_poll,lexer_max_gap;
static uint8_t lookup_probe,lookup_refs_page;
static unsigned long lookup_reads,lookup_reference_reads;
static uint8_t wire_lookup_probe,wire_lookup_page;
static unsigned long wire_lookup_reads;
static uint8_t source_edit_probe,source_edit_page;
static unsigned long source_edit_reads,source_edit_writes;
static unsigned source_edit_marks;
static uint16_t source_edit_extent;
static uint8_t source_hash_probe,source_hash_page,source_hash_seen[PAGE_SIZE/8];
static uint16_t source_hash_extent;
static unsigned long source_hash_reads,source_hash_duplicates,source_hash_writes,source_hash_marks;
static uint8_t stream_probe,stream_page_index[64],stream_seen[3*PAGE_SIZE/8];
static uint16_t stream_length,stream_zero,stream_zero_bytes;
static unsigned long stream_reads,stream_spans;static uint16_t stream_span_max;
static int stream_read_fail_after=-1;
static unsigned stream_copy_calls;
static uint8_t stream_copy_trace,stream_copy_pages[2048];
static uint16_t stream_copy_offsets[2048],stream_failed_offset;
static uint8_t stream_failed_page;
static uint8_t cancel_stage,cancel_result;
static unsigned poll_calls,spill_polls;
/* Probe only the initialization fixtures; the existing corpus is unchanged. */
static uint8_t initialization_probe,copy_started,copy_complete;
static uint8_t initialized[64],image_written[64][PAGE_SIZE/8];
static uint16_t copy_progress;
static unsigned long image_stores;
static unsigned image_fill_calls,image_fill_bytes,initialization_polls,image_fill_total_calls,image_fill_last_poll;
static uint32_t initialization_poll_stores[1024];
static int image_fill_fail_after=-1;
static unsigned image_fill_cancel_after;
static uint8_t image_fill_cancel_index,image_fill_cancel_page;
static uint8_t image_fill_failed_page,image_fill_failed_bytes[FAR_FILL_LIMIT];
static uint16_t image_fill_failed_offset,image_fill_failed_count;
static uint32_t image_fill_failed_version;
static uint8_t image_fill_indexes[256];
static uint16_t image_fill_offsets[256],image_fill_counts[256];
static void initialization_reset(void) {
    copy_started=copy_complete=0;copy_progress=0;image_stores=0;
    image_fill_calls=image_fill_bytes=initialization_polls=image_fill_last_poll=0;
}
uint8_t key_interrupt(void) {
    uint8_t reached;
    ++poll_calls;
    if(initialization_probe) {
        assert(initialization_polls<1024);
        initialization_poll_stores[initialization_polls++]=(uint32_t)image_stores;
    }
    if(lexer_poll_probe) {
        uint16_t gap=cc.scan-lexer_last_poll;
        if(gap>lexer_max_gap)lexer_max_gap=gap;
        lexer_last_poll=cc.scan;
    }
    reached=cancel_stage==1||
        (cancel_stage==2&&cc.image->pages[0]!=255)||
        (cancel_stage==3&&cc.emitted>CC_CODE_BASE)||
        (cancel_stage==4&&cc.image->map_length!=0)||
        (cancel_stage==5&&cc.scan>512)||
        (cancel_stage==6&&cc.sources->refs_owned&&cc.sources->refs_page!=255&&++spill_polls==2)||
        (cancel_stage==7&&compiler_invocation&&cc.sources->stage_shared&&cc.emitted>0xc100u);
    if(cancel_stage==8&&initialization_probe&&copy_started&&!copy_complete&&copy_progress>=256)reached=1;
    if(cancel_stage==9&&initialization_probe&&image_fill_calls>=image_fill_cancel_after&&
       cc.image->pages[image_fill_cancel_index]!=255&&!initialized[cc.image->pages[image_fill_cancel_index]]) {
        image_fill_cancel_page=cc.image->pages[image_fill_cancel_index];reached=1;
    }
    return reached?cancel_result:0;
}
uint8_t page_new(uint8_t role) {
    uint8_t i;
    ++allocation_calls;
    if(fail_after==0) return 255;
    if(fail_after>0) --fail_after;
    for(i=32;i<64;++i) if(!owned[i]) {
        owned[i]=role;lifetimes[i]=++next_lifetime;
        if(initialization_probe&&role==PAGE_IMAGE) {
            memset(memory[i],0xa5,PAGE_SIZE);memset(image_written[i],0,sizeof(image_written[i]));initialized[i]=0;
        }
        if(compiler_allocating==COMPILER_ALLOC_WORKER)compiler_owned[i>>3]|=1u<<(i&7);
        if(compiler_allocating==COMPILER_ALLOC_PARENT)compiler_parent_owned[i>>3]|=1u<<(i&7);
        return i;
    }
    return 255;
}
uint8_t page_drop(uint8_t p) {
    assert(p<64&&owned[p]);if(p==fail_drop)return 0;
    if(compiler_allocating==COMPILER_ALLOC_WORKER&&!(compiler_owned[p>>3]&(1u<<(p&7))))return 0;
    owned[p]=0;compiler_owned[p>>3]&=~(1u<<(p&7));compiler_parent_owned[p>>3]&=~(1u<<(p&7));return 1;
}
uint8_t page_role(uint8_t p) { return p<64?owned[p]:0; }
uint32_t page_lifetime(uint8_t p) { return p<64&&owned[p]?lifetimes[p]:0; }
typedef struct {const char *path,*text;} HostSource;
static const HostSource *disk_sources;
static unsigned disk_source_count,disk_reads;
uint8_t read_block(Source *source,const char *name,Diagnostic *diagnostic) {
    char path[128];unsigned i;
    strcpy(path,name);memset(diagnostic,0,sizeof(*diagnostic));++disk_reads;
    for(i=0;i<disk_source_count;++i)if(!strcmp(path,disk_sources[i].path)) {
        memset(source,0,sizeof(*source));source->page=255;strcpy(source->path,path);
        if(source_import(source,(const uint8_t*)disk_sources[i].text,(uint16_t)strlen(disk_sources[i].text),diagnostic))return 1;
        if(source->page!=255)source_close(source);return 0;
    }
    return 0;
}
uint8_t pages_used(void) { uint8_t n=0,i; for(i=32;i<64;++i) n+=owned[i]!=0; return n; }
void page_initialized(uint8_t p,uint16_t n) {
    uint16_t at;
    assert(p<64&&owned[p]&&n<=PAGE_SIZE);
    if(source_edit_probe&&p==source_edit_page){++source_edit_marks;source_edit_extent=n;}
    if(source_hash_probe&&p==source_hash_page)++source_hash_marks;
    if(initialization_probe&&owned[p]==PAGE_IMAGE) {
        for(at=0;at<n;++at)assert(image_written[p][at>>3]&(1u<<(at&7)));
        if(p==cc.image->pages[0]&&copy_complete) {
            assert(!memcmp(memory[p],runtime_start,cc_runtime_size()));
            for(at=cc_runtime_size();at<n;++at)assert(memory[p][at]==0);
        } else for(at=0;at<n;++at)assert(memory[p][at]==0);
        initialized[p]=1;
    }
}
uint8_t far_get(uint8_t p,uint16_t at) { assert(p<64&&owned[p]&&at<PAGE_SIZE); if(reference_probe&&p==reference_probe_page)++reference_reads; if(capture_probe&&p==capture_probe_page)++capture_reads; if(lexer_probe&&p==lexer_probe_page){assert(at<256);++lexer_reads[at];} if(lookup_probe){++lookup_reads;if(p==lookup_refs_page)++lookup_reference_reads;} if(wire_lookup_probe&&p==wire_lookup_page)++wire_lookup_reads; if(source_edit_probe&&p==source_edit_page)++source_edit_reads; if(source_hash_probe&&p==source_hash_page){assert(at<source_hash_extent);if(source_hash_seen[at>>3]&(1u<<(at&7)))++source_hash_duplicates;source_hash_seen[at>>3]|=(uint8_t)(1u<<(at&7));++source_hash_reads;} if(stream_probe&&stream_page_index[p]!=255){uint32_t pos=(uint32_t)stream_page_index[p]*PAGE_SIZE+at;assert(pos<stream_length&&!(pos>=stream_zero&&pos-stream_zero<stream_zero_bytes));assert(!(stream_seen[pos>>3]&(1u<<(pos&7))));stream_seen[pos>>3]|=(uint8_t)(1u<<(pos&7));++stream_reads;} return memory[p][at]; }
void far_set(uint8_t p,uint16_t at,uint8_t v) {
    assert(p<64&&owned[p]&&at<PAGE_SIZE);++page_versions[p];memory[p][at]=v;
    if(source_edit_probe&&p==source_edit_page)++source_edit_writes;
    if(source_hash_probe&&p==source_hash_page)++source_hash_writes;
    if(initialization_probe&&owned[p]==PAGE_IMAGE) {
        ++image_stores;image_written[p][at>>3]|=(uint8_t)(1u<<(at&7));
        if(p==cc.image->pages[0]&&!copy_complete) {
            if(at==0&&v==runtime_start[0])copy_started=1;
            if(copy_started&&at<cc_runtime_size()) {
                copy_progress=at+1;
                if(copy_progress==cc_runtime_size()) {
                    assert(!memcmp(memory[p],runtime_start,cc_runtime_size()));copy_complete=1;
                }
            }
        }
    }
}
/* Host model of the already-native-proven read primitive. Near windows and
 * registers are hardware-only; this model enforces ownership/page extents. */
uint8_t far_read(uint8_t page,uint16_t offset,uint8_t *out,uint16_t n) {
    uint16_t i;
    if(!n)return 1;
    if(n>FAR_READ_LIMIT||page<32||page>=64||!owned[page]||offset>=PAGE_SIZE||n>PAGE_SIZE-offset||!out)return 0;
    if(stream_copy_trace) {
        assert(stream_copy_calls<2048);
        stream_copy_pages[stream_copy_calls]=page;stream_copy_offsets[stream_copy_calls]=offset;
    }
    ++stream_copy_calls;
    if(stream_read_fail_after==0){stream_failed_page=page;stream_failed_offset=offset;return 0;}
    if(stream_read_fail_after>0)--stream_read_fail_after;
    if(stream_probe){++stream_spans;if(n>stream_span_max)stream_span_max=n;assert(n<=64);}
    for(i=0;i<n;++i)out[i]=far_get(page,offset+i);
    return 1;
}
/* Native fill geometry/version behaviour was proved in PERF12. Reuse
 * far_set here so poison/write/version probes observe every modelled store. */
uint8_t far_fill(uint8_t page,uint16_t offset,uint8_t value,uint16_t n) {
    uint16_t i;
    if(!n)return 1;
    if(n>FAR_FILL_LIMIT||page<32||page>=64||!owned[page]||offset>=PAGE_SIZE||n>PAGE_SIZE-offset)return 0;
    if(initialization_probe) {
        assert(image_fill_calls<256&&owned[page]==PAGE_IMAGE&&value==0);
        for(i=0;i<3&&cc.image->pages[i]!=page;++i){}
        assert(i<3);image_fill_indexes[image_fill_calls]=(uint8_t)i;
        if(i!=0) {
            assert(initialization_polls>image_fill_last_poll);
            assert(initialization_poll_stores[initialization_polls-1]==image_stores);
        }
        image_fill_last_poll=initialization_polls;
        image_fill_offsets[image_fill_calls]=offset;image_fill_counts[image_fill_calls]=n;
        ++image_fill_calls;
    }
    ++image_fill_total_calls;
    if(image_fill_fail_after==0) {
        image_fill_failed_page=page;image_fill_failed_offset=offset;image_fill_failed_count=n;
        image_fill_failed_version=page_versions[page];
        memcpy(image_fill_failed_bytes,memory[page]+offset,n);return 0;
    }
    if(image_fill_fail_after>0)--image_fill_fail_after;
    for(i=0;i<n;++i)far_set(page,offset+i,value);
    if(initialization_probe)image_fill_bytes+=n;
    return 1;
}
#include "far-move-model.h"
static unsigned source_move_calls;
static uint16_t source_move_from[64],source_move_to[64],source_move_count[64];
uint8_t far_move(uint8_t page,uint16_t to,uint16_t from,uint16_t count) {
    if(source_edit_probe) {
        assert(page==source_edit_page&&source_move_calls<64);
        source_move_from[source_move_calls]=from;source_move_to[source_move_calls]=to;
        source_move_count[source_move_calls++]=count;
    }
    return model_far_move(page,to,from,count);
}
static Source s;
static Build b;
static Diagnostic d;
static void checked(int value,unsigned line) {
    ++checks;
    if(!value) {
        fprintf(stderr,"check %u, line %u, diagnostic %u at %u, emitted=%u static=%u temporaries=%u\n",
                checks,line,d.code,d.start,cc.emitted,b.globals_bytes,cc.temporaries);
        abort();
    }
}
#define check(value) checked((value),__LINE__)
static void load(const char *text) { check(source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d)); }
static void invalid(const char *text,uint8_t code) {
    uint8_t ok;
    load(text); ok=compile(&s,&b,&d);
    if(ok||d.code!=code) fprintf(stderr,"expected diagnostic %u: %s\n",code,text);
    check(!ok&&d.code==code); check(pages_used()==1);
}
/* Direct scanner checks isolate identifier source reads from symbol lookup. */
static void lexer_case(const char *label,const char *text,const char *expected,uint16_t token,uint8_t bracket) {
    CompilerSources sources;uint16_t i,length=(uint16_t)strlen(expected);unsigned long reads=0,lookahead=0;
    load(text);memset(&cc,0,sizeof(cc));memset(&sources,0,sizeof(sources));
    cc.input=&s;cc.error=&d;cc.sources=&sources;
    lexer_probe_page=s.page;memset(lexer_reads,0,sizeof(lexer_reads));lexer_probe=1;
    cc_next();lexer_probe=0;
    check(!d.code&&cc.token==token&&!strcmp(cc.name,expected)&&cc.scan==length&&
          cc.token_start==0&&cc.token_end==length);
    for(i=0;i<length;++i){check(lexer_reads[i]!=0);reads+=lexer_reads[i];}
    for(;i<256;++i)lookahead+=lexer_reads[i];
    printf("PERF04 %s bytes=%u identifier-reads=%lu lookahead-reads=%lu\n",label,length,reads,lookahead);
    if(bracket) {
        uint16_t start=(uint16_t)(strstr(text,".)")-text);
        cc_next();check(!d.code&&cc.token==']'&&cc.scan==start+2&&cc.token_start==start&&cc.token_end==start+2);
    }
    cc_next();check(!d.code&&cc.token==T_EOF&&cc.scan==s.length&&cc.token_start==s.length&&cc.token_end==s.length);
    memset(&cc,0,sizeof(cc));
}
static void lexer_cases(void) {
    char letters[128],digits[128],expected[128],bracket[131],cancel[640];uint16_t i;uint8_t reason;
    CompilerSources sources;
    lexer_case("mixed","Alpha_09.)","alpha_09",T_ID,1);
    lexer_case("alternating","A0B1_C2","a0b1_c2",T_ID,0);
    lexer_case("reserved","BeGiN.)","begin",T_RESERVED,1);
    lexer_case("underscore","_Name0","_name0",T_ID,0);
    for(i=0;i<127;++i){letters[i]=i&1?'z':'A';expected[i]=i&1?'z':'a';}
    letters[127]=expected[127]=0;
    lexer_case("letters127",letters,expected,T_ID,0);
    memcpy(bracket,letters,127);memcpy(bracket+127,"\n.)",4);
    lexer_case("letters127-bracket",bracket,expected,T_ID,1);
    for(i=0;i<127;++i){digits[i]=i?'0'+i%10:'A';expected[i]=i?digits[i]:'a';}
    digits[127]=expected[127]=0;
    lexer_case("digits127",digits,expected,T_ID,0);
    /* Raw scanner-only input deliberately bypasses the public127-column
     * source guard. Span several full poll intervals and prove that bytes
     * beyond the name buffer are consumed without extending significance. */
    load("a");for(i=0;i<1025;++i)far_set(s.page,i,'a');s.length=1025;page_initialized(s.page,s.length);
    for(reason=0;reason<2;++reason) {
        if(reason)far_set(s.page,1024,'z');
        memset(&cc,0,sizeof(cc));memset(&sources,0,sizeof(sources));memset(&d,0,sizeof(d));
        cc.input=&s;cc.error=&d;cc.sources=&sources;cc.polling=1;
        lexer_last_poll=lexer_max_gap=0;poll_calls=0;lexer_poll_probe=1;
        cc_next();lexer_poll_probe=0;
        check(!d.code&&cc.token==T_ID&&cc.scan==1025&&cc.token_start==0&&cc.token_end==1025&&
              strlen(cc.name)==127&&poll_calls>=3&&lexer_max_gap<=256);
        for(i=0;i<127;++i)check(cc.name[i]=='a');
        printf("PERF04 raw1025 variant=%u polls=%u max-byte-gap=%u\n",reason,poll_calls,lexer_max_gap);
        cc_next();check(!d.code&&cc.token==T_EOF&&cc.token_start==1025&&cc.token_end==1025);
        memset(&cc,0,sizeof(cc));
    }
    /* Prime the existing byte poll near wrap. Both cancellation reasons must
     * be observed inside the identifier with a bounded source-byte gap. */
    memset(cancel,'\n',512);memcpy(cancel+512,letters,128);
    for(reason=1;reason<=2;++reason) {
        load(cancel);memset(&cc,0,sizeof(cc));memset(&sources,0,sizeof(sources));
        cc.input=&s;cc.error=&d;cc.sources=&sources;cc.scan=512;cc.scan_poll=200;cc.polling=1;
        lexer_last_poll=512;lexer_max_gap=0;poll_calls=0;cancel_stage=5;cancel_result=reason;lexer_poll_probe=1;
        cc_next();lexer_poll_probe=0;cancel_stage=0;
        check(d.code==COMPILE_INTERRUPTED&&cc.interrupted==reason&&cc.scan==639&&cc.token_end==639&&
              poll_calls>=2&&lexer_max_gap>0&&lexer_max_gap<=256);
        cc_next();check(cc.token==T_EOF&&d.code==COMPILE_INTERRUPTED);
        memset(&cc,0,sizeof(cc));
    }
}
static void lookup_symbol(uint16_t id,uint8_t role,uint8_t owner,uint16_t reference,uint8_t length,uint16_t address) {
    Symbol *symbol=&cc.symbols[id];memset(symbol,0,sizeof(*symbol));
    symbol->role=role;symbol->owner=owner;symbol->name_start=reference;symbol->name_length=length;symbol->address=address;
    if(id>=cc.symbol_count)cc.symbol_count=id+1;
}
static void lookup_meter(const char *label,uint8_t field,int16_t expected) {
    int16_t actual;
    lookup_reads=lookup_reference_reads=0;lookup_probe=1;
    actual=field?cc_field_lookup(0):cc_lookup();lookup_probe=0;
    check(actual==expected&&!d.code);
    printf("PERF05 %s reads=%lu reference-reads=%lu\n",label,lookup_reads,lookup_reference_reads);
}
/* Isolate production name comparisons with real catalog/lifetime checks. */
static void lookup_cases(void) {
    CompilerSources state;Build image;Source included;uint8_t hash[32],refs_page;uint16_t refs[6],id;
    uint32_t version;char text[285];
    strcpy(text,"AbCdEfGhIj\nAbCdEfGhiK\nInPuT\n");
    memset(text+28,'a',127);text[154]='x';text[155]='\n';
    memset(text+156,'a',127);text[282]='y';text[283]='\n';text[284]=0;load(text);
    memset(&cc,0,sizeof(cc));memset(&state,0,sizeof(state));memset(&image,0,sizeof(image));memset(&d,0,sizeof(d));
    cc.sources=&state;cc.image=&image;cc.input=&s;cc.error=&d;
    check(sources_new(&image.sources));test_source_hash(&s,hash);check(sources_borrow(&image.sources,&s)==0);
    check(source_new(&included,"H:/PART.INC"));check(source_import(&included,(const uint8_t*)"iNcLuDe999",10,&d));
    test_source_hash(&included,hash);check(sources_borrow(&image.sources,&included)==1);
    refs_page=page_new(PAGE_COMPILER);check(refs_page!=255);state.refs_page=refs_page;
    refs[0]=cc_source_reference(0);refs[1]=cc_source_reference(11);refs[2]=cc_source_reference(22);
    refs[3]=cc_source_reference(28);refs[4]=cc_source_reference(156);refs[5]=cc_source_reference(0x10000UL);
    check(!d.code&&state.refs==6);lookup_refs_page=refs_page;
    cc.workspace=page_new(PAGE_COMPILER);check(cc.workspace!=255);
    for(id=PAGE_SIZE-3*ROUTINE_BYTES;id<PAGE_SIZE;++id)far_set(cc.workspace,id,0);
    cc_rput(1,R_PARENT,0);cc_rput(2,R_PARENT,1);cc_rput(3,R_PARENT,0);
    cc.token=T_ID;strcpy(cc.name,"abcdefghij");
    lookup_symbol(0,S_VARIABLE,0,refs[0],10,0);
    lookup_meter("symbol10",0,0);
    lookup_symbol(0,S_FIELD,0,refs[0],10,7);cc.types[0].low=-1;
    lookup_meter("field10",1,0);check(cc.field_offset==7);
    lookup_symbol(0,S_VARIABLE,0,refs[5],10,0);strcpy(cc.name,"include999");
    lookup_meter("include10",0,0);
    strcpy(cc.name,"a");lookup_symbol(0,S_VARIABLE,0,refs[0],1,0);lookup_meter("symbol1",0,0);
    strcpy(cc.name,"abcdefghij");
    lookup_symbol(0,S_VARIABLE,0,refs[0],10,0);lookup_symbol(1,S_VARIABLE,1,refs[0],10,0);
    lookup_symbol(2,S_VARIABLE,2,refs[0],10,0);cc.scope=2;check(cc_lookup()==2&&!d.code);
    cc.symbols[2].name_start=refs[1];check(cc_lookup()==1&&!d.code);
    cc.symbols[1].name_start=refs[1];check(cc_lookup()==0&&!d.code);cc.scope=3;check(cc_lookup()==0&&!d.code);
    cc.scope=0;cc.symbol_count=0;lookup_symbol(0,S_FIELD,0,refs[0],10,7);
    lookup_symbol(1,S_FIELD,1,refs[0],10,9);lookup_symbol(2,S_VARIABLE,0,refs[0],10,0);
    cc.types[0].low=cc.types[1].low=-1;cc.with_depth=2;cc.with_types[0]=0;cc.with_types[1]=1;
    check(cc_lookup()==1&&cc.field_offset==9&&!d.code);
    cc.symbols[1].name_start=refs[1];check(cc_lookup()==0&&cc.field_offset==7&&!d.code);
    cc.symbols[1].owner=0;cc.symbols[1].name_start=refs[0];cc.with_depth=0;
    check(cc_field_lookup(0)==0&&cc.field_offset==7&&!d.code);
    cc.symbols[1].name_start=state.refs;check(cc_field_path(0,1)==9&&!d.code);
    /* Variant recursion must retain the selected field and accumulated offset. */
    cc.symbol_count=0;lookup_symbol(0,S_FIELD,1,refs[0],10,7);lookup_symbol(1,S_VARIABLE,0,0,0,0);
    lookup_symbol(2,S_VARIANT,0,0,0,4);lookup_symbol(3,S_VLABEL,0,0,0,0);cc.symbols[3].type=1;cc.types[0].low=2;
    check(cc_field_lookup(0)==0&&cc.field_offset==11&&!d.code);
    cc.symbols[0].name_start=state.refs;check(cc_field_path(0,0)==11&&!d.code);
    /* Keep special-name fallback and ordinary shadowing in their original order. */
    cc.symbol_count=cc.type_count=0;cc.text_type=255;cc.scope=0;cc_system_init();strcpy(cc.name,"input");
    id=(uint16_t)cc_lookup();check(id<cc.symbol_count&&cc.symbols[id].role==S_STANDARD&&!d.code);
    id=cc.symbol_count;lookup_symbol(id,S_VARIABLE,0,refs[2],5,0);check(cc_lookup()==id&&!d.code);
    lookup_symbol(id+1,S_VARIABLE,1,refs[2],5,0);cc.scope=1;check(cc_lookup()==id+1&&!d.code);
    cc.scope=0;check(cc_lookup()==id&&!d.code);
    /* Every legal byte remains significant, including the last common-prefix byte. */
    cc.symbol_count=0;lookup_symbol(0,S_VARIABLE,0,refs[3],127,0);lookup_symbol(1,S_VARIABLE,0,refs[4],127,0);
    memset(cc.name,'a',126);cc.name[126]='x';cc.name[127]=0;lookup_meter("prefix127",0,0);
    cc.name[126]='y';check(cc_lookup()==1&&!d.code);cc.name[126]='z';check(cc_lookup()==-1&&!d.code);
    /* Length mismatch and explicit field paths do not dereference a name. */
    cc.symbol_count=1;lookup_symbol(0,S_VARIABLE,0,state.refs,9,0);strcpy(cc.name,"abcdefghij");
    check(cc_lookup()==-1&&!d.code);cc.symbols[0].name_length=10;check(cc_lookup()==0&&d.code==99);d.code=0;
    state.refs_page=255;cc.symbols[0].name_start=refs[0];check(cc_lookup()==0&&d.code==99);d.code=0;state.refs_page=refs_page;
    cc.symbols[0].name_length=0;cc.symbols[0].name_start=state.refs;cc.name[0]=0;check(cc_lookup()==0&&!d.code);
    strcpy(cc.name,"abcdefghij");cc.symbols[0].name_start=refs[0];cc.symbols[0].name_length=10;
    version=page_versions[image.sources.page];++page_versions[image.sources.page];check(cc_lookup()==-1&&!d.code);
    page_versions[image.sources.page]=version;
    /* Function-result collisions compare two stored names, not cc.name. */
    cc.scope=1;cc.symbol_count=0;lookup_symbol(0,S_VARIABLE,1,refs[0],10,0);lookup_symbol(1,S_FUNCTION,0,refs[0],10,0);
    cc.types[0].family=K_INTEGER;cc.types[0].width=2;lookup_reads=lookup_reference_reads=0;lookup_probe=1;
    cc_result_symbol(1);lookup_probe=0;check(d.code==43&&cc.symbol_count==2&&!image.globals_bytes);
    printf("PERF05 result10 reads=%lu reference-reads=%lu\n",lookup_reads,lookup_reference_reads);d.code=0;
    cc.symbols[0].name_start=refs[1];cc_result_symbol(1);check(!d.code&&cc.symbol_count==3&&cc.symbols[2].role==S_RESULT&&image.globals_bytes==2);
    cc.symbol_count=2;image.globals_bytes=0;cc.symbols[0].name_start=state.refs;cc_result_symbol(1);check(d.code==99&&cc.symbol_count==2);d.code=0;
    cc.symbols[0].name_start=refs[0];cc.symbols[1].name_start=state.refs;cc_result_symbol(1);check(d.code==99&&cc.symbol_count==2);d.code=0;
    cc.symbols[0].name_length=cc.symbols[1].name_length=0;cc_result_symbol(1);check(d.code==43&&cc.symbol_count==2);d.code=0;
    cc.symbols[0].name_length=9;cc.symbols[1].name_length=10;cc_result_symbol(1);check(!d.code&&cc.symbol_count==3);
    check(page_drop(cc.workspace)&&page_drop(refs_page));check(sources_close(&image.sources));source_close(&included);
    check(pages_used()==1);memset(&cc,0,sizeof(cc));
}
/* Raw-only hash fixtures bypass import normalization/line limits to preserve
 * the existing hasher contract for every byte. Expected digests come from
 * Python hashlib over independently normalized byte strings. */
static void source_hash_case(const char *label,const uint8_t *raw,uint16_t n,const char *expected) {
    uint8_t digest[32],original[PAGE_SIZE];char actual[65];uint16_t i;
    Source before;uint32_t version,lifetime;unsigned allocations;
    load("");for(i=0;i<n;++i)far_set(s.page,i,raw[i]);s.length=n;page_initialized(s.page,n);
    s.cursor=n/2;s.previous=n;s.dirty=1;before=s;memcpy(original,memory[s.page],PAGE_SIZE);
    version=page_versions[s.page];lifetime=page_lifetime(s.page);allocations=allocation_calls;
    source_hash_page=s.page;source_hash_extent=n;memset(source_hash_seen,0,sizeof(source_hash_seen));
    source_hash_reads=source_hash_duplicates=source_hash_writes=source_hash_marks=0;source_hash_probe=1;
    test_source_hash(&s,digest);source_hash_probe=0;
    for(i=0;i<32;++i)snprintf(actual+2*i,3,"%02x",digest[i]);
    check(!strcmp(actual,expected));
    check(!memcmp(&s,&before,sizeof(s))&&!memcmp(original,memory[s.page],PAGE_SIZE));
    check(!source_hash_writes&&!source_hash_marks&&page_versions[s.page]==version&&
        page_lifetime(s.page)==lifetime&&allocation_calls==allocations);
    check(source_hash_reads==n&&!source_hash_duplicates);
    printf("PERF09 %s bytes=%u reads=%lu duplicate-reads=%lu\n",label,n,source_hash_reads,source_hash_duplicates);
}
static void source_hash_cases(void) {
    uint8_t raw[SOURCE_LIMIT];uint16_t i;
    static const uint8_t vector_0[]={0};
    static const uint8_t vector_1[]={32,32,32};
    static const uint8_t vector_2[]={32,10,32,32,10,32};
    static const uint8_t vector_3[]={32,32,97,108,112,104,97,32,98,101,116,97,32,32,32,10,32,103,97,109,109,97,32,32};
    static const uint8_t vector_4[]={9,32,9,32,32,10,32,9,32};
    static const uint8_t vector_5[]={128,32,255,32,32,10,32,0,32,13,32,26,32};
    static const uint8_t vector_6[]={120,32,10};
    static const uint8_t vector_7[]={120,32};
    static const uint8_t vector_8[]={32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32};
    static const uint8_t vector_9[]={32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98};
    static const uint8_t vector_10[]={32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32};
    static const uint8_t vector_11[]={32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98};
    static const uint8_t vector_12[]={32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32,97,32,98,32};
    static const uint8_t vector_13[]={17,90,163,236,53,126,199,16,89,162,235,52,125,198,15,88,161,234,51,124,197,14,87,160,233,50,123,196,13,86,159,232,49,122,195,12,85,158,231,48,121,194,11,84,157,230,47,120,193,10,83,156,229,46,119,192,9,82,155,228,45,118,191,8,81,154,227,44,117,190,7,80,153,226,43,116,189,6,79,152,225,42,115,188,5,78,151,224,41,114,187,4,77,150,223,40,113,186,3,76,149,222,39,112,185,2,75,148,221,38,111,184,1,74,147,220,37,110,183,0,73,146,219,36,109,182,255,72,145,218,35,108,181,254,71,144,217,34,107,180,253,70,143,216,33,106,179,252,69,142,215,32,105,178,251,68,141,214,31,104,177,250,67,140,213,30,103,176,249,66,139,212,29,102,175,248,65,138,211,28,101,174,247,64,137,210,27,100,173,246,63,136,209,26,99,172,245,62,135,208,25,98,171,244,61,134,207,24,97,170,243,60,133,206,23,96,169,242,59,132,205,22,95,168,241,58,131,204,21,94,167,240,57,130,203,20,93,166,239,56,129,202,19,92,165,238,55,128,201,18,91,164,237,54,127,200,17};
    static const uint8_t canonical_55[]={97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,32,98};
    static const uint8_t canonical_63[]={97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,32,98};
    static const uint8_t canonical_65[]={97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,97,32,32,32,98};
    source_hash_case("empty",vector_0,0,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    source_hash_case("all-space",vector_1,3,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    source_hash_case("blank-lines",vector_2,6,"75a11da44c802486bc6f65640aa48a730f0f684c5c07a42ba3cd1735eb3fb070");
    source_hash_case("interior-spaces",vector_3,24,"23429447996324adc4ad60bf1e1de7532337babb3b90bc2bb02dc8776d17ca11");
    source_hash_case("tabs",vector_4,9,"879fe5d060271e3521dc1e97b1b7f9a6929efe099103d22b3b48c1578ab4abbc");
    source_hash_case("binary-controls",vector_5,13,"115809f719f6506c6e100ce56cd0dadb942cf290721abd9078c15cd3421fc140");
    source_hash_case("final-newline",vector_6,3,"73cb3858a687a8494ca3323053016282f3dad39d42cf62ca4e79dda2aac7d9ac");
    source_hash_case("no-final-newline",vector_7,2,"2d711642b726b04401627ca9fbac32f5c8530fb1903cc4db02258717921a4881");
    source_hash_case("block-55",vector_8,55,"8d85ff2bd14eb80625a88055ac6071b48dc0b5748055770f38ff3339b767082a");
    source_hash_case("block-56",vector_9,56,"5490039aeb46672925c5767230093bc6099f06d66b70976b71dbaed22c5f1824");
    source_hash_case("block-63",vector_10,63,"440761b008fb221ef0e14bf80b28b2e183a798e3bb88a6de2040a1afe06f1bd8");
    source_hash_case("block-64",vector_11,64,"3d5bd73950914060a01be61f695b319a57d4bff8542dcf88eefed98dc3c94b4d");
    source_hash_case("block-65",vector_12,65,"3d5bd73950914060a01be61f695b319a57d4bff8542dcf88eefed98dc3c94b4d");
    source_hash_case("byte-range",vector_13,257,"0669138e7a4c6b769eb3935c552bffe489453bb4fd1352096ad6a6380c639c55");
    source_hash_case("canonical-block-55",canonical_55,55,"7467d2f61bd62799ed235992776dd0889feeca1e5598dbe982365f9a5df1429d");
    source_hash_case("canonical-block-63",canonical_63,63,"c1fb4166308b86d081687d34cc8221bd6939c0ee154651c40b89fc8a30b01309");
    source_hash_case("canonical-block-65",canonical_65,65,"3ed4e99289e2a0d203a77917b2cd75ffa7295724c357cb4bd11717fe061c40b3");
    for(i=0;i<16000;++i)raw[i]=i%100==99?'\n':'a';
    source_hash_case("canonical16000",raw,16000,"0bcd8a336635fc870bd10943c14038278c7e7bbd7225d32541369d3ba9baf5e2");
    memset(raw,' ',16000);
    source_hash_case("all-space16000",raw,16000,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    memset(raw,' ',16000);raw[15999]=255;
    source_hash_case("long-pending",raw,16000,"fa6e500bb9b9215b384b51c1ecf0a8f0aa92d22db490887276953075e86a9db5");
    memset(raw,' ',16000);raw[0]='a';
    source_hash_case("long-trailing",raw,16000,"ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");
    memset(raw,' ',SOURCE_LIMIT);
    source_hash_case("raw-source-limit",raw,16383,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}
/* Raw stream oracle vectors come from Python hashlib, independently of
 * either C implementation. Probes forbid reads from normalized zero windows. */
static uint8_t stream_byte(uint32_t i) {return (uint8_t)(i*29u+(i/PAGE_SIZE)*7u+3u);}
static void stream_case(const char *label,const uint8_t pages[3],uint16_t length,
                        uint16_t zero,uint16_t zero_bytes,const char *expected,
                        uint16_t reads,uint16_t spans) {
    JobStream stream,before;uint8_t digest[32],j,intact=1;char actual[65];uint16_t i;
    uint32_t versions[3],lifetimes[3];unsigned allocations=allocation_calls;uint8_t owned_pages=pages_used();
    memset(&stream,0,sizeof(stream));stream.length=length;stream.count=length?(length+PAGE_SIZE-1u)/PAGE_SIZE:0;
    memcpy(stream.pages,pages,3);before=stream;
    memset(stream_page_index,255,sizeof(stream_page_index));
    for(j=0;j<3;++j){stream_page_index[pages[j]]=j;versions[j]=page_versions[pages[j]];lifetimes[j]=page_lifetime(pages[j]);}
    stream_length=length;stream_zero=zero;stream_zero_bytes=zero_bytes;
    memset(stream_seen,0,sizeof(stream_seen));stream_reads=stream_spans=0;stream_span_max=0;stream_probe=1;
    check(job_hash_stream(&stream,zero,zero_bytes,digest));stream_probe=0;
    for(i=0;i<32;++i)snprintf(actual+2*i,3,"%02x",digest[i]);
    check(!strcmp(actual,expected)&&stream_reads==reads);
    check(!memcmp(&stream,&before,sizeof(stream))&&allocation_calls==allocations&&pages_used()==owned_pages);
    for(j=0;j<3;++j){
        check(page_versions[pages[j]]==versions[j]&&page_lifetime(pages[j])==lifetimes[j]);
        for(i=0;i<PAGE_SIZE;++i)if(memory[pages[j]][i]!=stream_byte((uint32_t)j*PAGE_SIZE+i))intact=0;
    }
    check(intact);check(stream_spans==spans&&stream_span_max<=64);
    printf("PERF11 %s bytes=%u reads=%lu spans=%lu max=%u\n",label,length,stream_reads,stream_spans,stream_span_max);
}
static void stream_hash_cases(void) {
    uint8_t pages[3],j;uint16_t i;unsigned prior=pages_used();
    for(j=0;j<3;++j){pages[j]=page_new(PAGE_IMAGE);check(pages[j]!=255);for(i=0;i<PAGE_SIZE;++i)far_set(pages[j],i,stream_byte((uint32_t)j*PAGE_SIZE+i));page_initialized(pages[j],PAGE_SIZE);}
    stream_case("empty",pages,0,0,0,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",0,0);
    stream_case("one",pages,1,0,0,"084fed08b978af4d7d196a7446a86b58009e636b611db16211b65a9aadff29c5",1,1);
    stream_case("raw-55",pages,55,0,0,"c68cdcb6098d4e6b8f496f90b16425b09df4ac7068660de2ddbe41c39dba6c4b",55,1);
    stream_case("raw-56",pages,56,0,0,"46e33f2b906a6dd98e35898ca8f5420213694109ed1b58818f9462e8775568b2",56,1);
    stream_case("raw-63",pages,63,0,0,"a6f28405393c193e468b6702780ca882414ccf29dd6925415d40c2ffc6601cf8",63,1);
    stream_case("raw-64",pages,64,0,0,"3ce444057f0b62607535bf4c7d980854756b87fc42c522bbb4243c8878a275c9",64,1);
    stream_case("raw-65",pages,65,0,0,"7d33f72234ffc80b34b2e78e344186b8c434519bda30170e294d41f0dae39438",65,2);
    stream_case("raw-128",pages,128,0,0,"c7c1c41a8b6f4ee193dcf291b8fb5717b0671ad4a1d560b18b8419994e043fcc",128,2);
    stream_case("raw-129",pages,129,0,0,"ab99cfb193e986c559090f884141792caf332786c9f31f03bcb0c02648afe14e",129,3);
    stream_case("raw-16383",pages,16383,0,0,"f73c0cd80075394dc0002d01359b0159ff77e047e4713efd009e0cefb1421e81",16383,256);
    stream_case("raw-16384",pages,16384,0,0,"865aef066298849fa264749479a68ab328451c8f8732f52d4cd51d8c593e2122",16384,256);
    stream_case("raw-16385",pages,16385,0,0,"c91cbb54d14995be85939e6deef158a3de6a4e969cc4f31344048d6314a8242e",16385,257);
    stream_case("raw-49152",pages,49152,0,0,"2d64e9f75f9316f18327945f8da8d9c4d925821cd5d3a038c68c9a036c93aed7",49152,768);
    stream_case("zero-prefix",pages,129,0,32,"2db4778c057d56d46abf7515c83599268b3741644034658e79beaefa5f77c955",97,2);
    stream_case("zero-suffix",pages,129,97,32,"8ade5217b5e064e3084afc889bd655831f6f586d9aebc990118e96c074a075f4",97,2);
    stream_case("zero-interior",pages,129,31,66,"912a2c2cd2ce99f9917b2077ba59ee5bd6e681de85cef66f0f5376981e3e227b",63,2);
    stream_case("zero-block-edge",pages,129,63,2,"9fd481e9b1d5205812cad3ac4a64d4e1b9af682a76f185c3213b139d83185c72",127,2);
    stream_case("zero-page-edge",pages,32770,16380,64,"cbea304611b917369eaa2d0384c6765df32967e2dd0eb2938e9b83a4519faeec",32706,513);
    stream_case("zero-middle-page",pages,49152,16384,16384,"bce8c216560337c54f29b0f1ae871041738a7cf8db5a0e39ad33b9326f2812b9",32768,512);
    stream_case("zero-all",pages,49152,0,65535,"2aae7dc846aaf25f1cadf55f1666862046c6db9d65d84bdc07fa039dac405606",0,0);
    stream_case("zero-outside",pages,129,65535,65535,"ab99cfb193e986c559090f884141792caf332786c9f31f03bcb0c02648afe14e",129,3);
    stream_case("zero-end-overflow",pages,49152,32760,65535,"4ea725655fa0a201f13f8cc4653a6c18894d46c7032c43237e4b0353d7574fff",32760,512);
    stream_case("zero-length-window",pages,129,31,0,"ab99cfb193e986c559090f884141792caf332786c9f31f03bcb0c02648afe14e",129,3);
    /* Rejected metadata and failed copies never publish a partial digest. */
    {
        JobStream stream;uint8_t digest[32],sentinel[32];unsigned k;
        memset(&stream,0,sizeof(stream));memcpy(stream.pages,pages,3);
        memset(sentinel,0xa5,sizeof(sentinel));
        for(k=0;k<6;++k) {
            stream.length=k==0?49153UL:129;stream.count=k==1?0:1;
            stream.pages[0]=k==2?255:pages[0];
            stream_read_fail_after=k>=3?(int)(k-3):-1;
            memcpy(digest,sentinel,32);
            check(!job_hash_stream(&stream,0,0,digest));
            check(!memcmp(digest,sentinel,32));
            stream_read_fail_after=-1;
        }
    }
    for(j=0;j<3;++j)check(page_drop(pages[j]));check(pages_used()==prior);
}
/* Compare the production range chunker with libc's whole-range memmove. */
static void range_move_case(uint8_t owned_page,uint8_t page,uint16_t to,uint16_t from,uint16_t n,uint8_t ok) {
    uint8_t expected[PAGE_SIZE];uint16_t i;uint32_t version,lifetime=page_lifetime(owned_page);
    unsigned allocations=allocation_calls;uint8_t pages=pages_used();
    for(i=0;i<PAGE_SIZE;++i)far_set(owned_page,i,stream_byte(i));
    memcpy(expected,memory[owned_page],PAGE_SIZE);
    if(ok&&n)memmove(expected+to,expected+from,n);
    version=page_versions[owned_page];source_edit_page=owned_page;
    source_edit_reads=source_edit_writes=source_edit_marks=source_move_calls=0;source_edit_probe=1;
    check(source_move_range(page,to,from,n)==ok);source_edit_probe=0;
    check(!memcmp(expected,memory[owned_page],PAGE_SIZE)&&page_versions[owned_page]==version+(ok?(uint32_t)n:0));
    check(source_edit_reads==(ok?n:0)&&source_edit_writes==(ok?n:0)&&!source_edit_marks);
    check(source_move_calls==(ok?(n+FAR_MOVE_LIMIT-1u)/FAR_MOVE_LIMIT:0));
    check(page_lifetime(owned_page)==lifetime&&allocation_calls==allocations&&pages_used()==pages);
}
static void range_move_cases(void) {
    static const struct {uint16_t to,from,n;uint8_t ok;} cases[]={
        {0,1,16383,1},{1,0,16383,1},{257,0,16000,1},{0,257,16000,1},
        {0,0,16384,1},{16000,15999,384,1},{65535,65535,0,1},
        {16384,0,1,0},{0,16384,1,0},{16383,0,2,0},{0,16383,2,0},
        {65535,0,1,0},{0,65535,1,0},{0,0,16385,0}};
    uint8_t p=page_new(PAGE_SOURCE);unsigned i;check(p!=255);
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
        range_move_case(p,p,cases[i].to,cases[i].from,cases[i].n,cases[i].ok);
    range_move_case(p,31,0,0,1,0);range_move_case(p,64,0,0,1,0);range_move_case(p,255,65535,65535,0,1);
    check(page_drop(p));
}
static void source_edit_reset(void) {
    source_edit_page=s.page;source_edit_reads=source_edit_writes=source_edit_marks=source_edit_extent=0;source_move_calls=0;
}
static void source_edit_case(const char *label,uint16_t a,uint16_t z,const char *replacement,uint16_t n,
                             uint16_t previous,uint16_t expected_previous,unsigned long expected_reads,unsigned long expected_writes) {
    char initial[16001],expected[SOURCE_LIMIT+1];uint16_t i,size=16000-(z-a)+n;
    Source before,wanted;uint32_t version,lifetime;unsigned allocations;
    for(i=0;i<16000;++i)initial[i]=i%100==99?'\n':'a';initial[16000]=0;
    load(initial);s.cursor=123;s.previous=previous;s.dirty=0;before=s;wanted=before;
    memcpy(expected,initial,a);memcpy(expected+a,replacement,n);memcpy(expected+a+n,initial+z,16000-z);
    wanted.length=size;wanted.cursor=a+n;wanted.previous=expected_previous;wanted.dirty=1;++wanted.revision;
    version=page_versions[s.page];lifetime=page_lifetime(s.page);allocations=allocation_calls;
    source_edit_reset();source_edit_probe=1;
    check(source_replace(&s,a,z,(const uint8_t*)replacement,n));source_edit_probe=0;
    check(!memcmp(&s,&wanted,sizeof(s))&&!memcmp(memory[s.page],expected,size));
    check(page_lifetime(s.page)==lifetime&&allocations==allocation_calls&&source_edit_marks==1&&source_edit_extent==size);
    check((uint32_t)(page_versions[s.page]-version)==source_edit_writes);
    check(source_edit_reads==expected_reads&&source_edit_writes==expected_writes);
    {
        uint16_t remaining=16000-z,at=n>z-a?16000:z,span;unsigned call=0;
        if(n!=z-a)while(remaining) {
            span=remaining>FAR_MOVE_LIMIT?FAR_MOVE_LIMIT:remaining;
            if(n>z-a)at-=span;
            check(call<source_move_calls&&source_move_from[call]==at&&
                  source_move_to[call]==at+n-(z-a)&&source_move_count[call]==span);
            if(n<z-a)at+=span;
            remaining-=span;++call;
        }
        check(source_move_calls==call);
    }
    printf("PERF07 %s reads=%lu writes=%lu\n",label,source_edit_reads,source_edit_writes);
}
static void source_edit_cases(void) {
    char initial[16001];uint8_t too_large[384],wide[257],bad[3]={'\r',26,'a'};uint16_t i;
    Source before;uint32_t version;unsigned allocations;
    source_edit_case("middle",8000,8001,"z",1,8000,8001,15999,1);
    source_edit_case("begin",0,1,"z",1,0,1,15999,1);
    source_edit_case("end",15999,16000,"z",1,65535,16000,15999,1);
    source_edit_case("wide",8000,8004,"zzzz",4,8002,8004,15996,4);
    source_edit_case("newline",8000,8001,"\n",1,12000,12000,15999,1);
    source_edit_case("growth",8000,8001,"zz",2,12000,12001,23998,8001);
    source_edit_case("shrink",8000,8002,"z",1,12000,11999,23996,7999);
    source_edit_case("delete",8000,8001,"",0,8000,8000,23998,7999);
    for(i=0;i<sizeof(wide);++i)wide[i]=i&1?'\n':'x';
    source_edit_case("wide-growth",8000,8001,(const char*)wide,257,12000,12256,23998,8256);
    source_edit_case("wide-shrink",8000,8513,"",0,12000,11487,22974,7487);
    source_edit_case("begin-insert",0,0,(const char*)wide,257,0,257,32000,16257);
    source_edit_case("end-insert",16000,16000,(const char*)wide,257,65535,16257,16000,257);
    source_edit_case("late-insert",15998,15998,(const char*)wide,257,15998,16255,16002,259);
    source_edit_case("begin-delete",0,1,"",0,65535,15999,31998,15999);
    for(i=0;i<16000;++i)initial[i]=i%100==99?'\n':'a';initial[16000]=0;load(initial);
    before=s;version=page_versions[s.page];allocations=allocation_calls;
    for(i=0;i<3;++i) {
        uint16_t at=i==2?99:8000;
        source_edit_reset();source_edit_probe=1;
        check(!source_replace(&s,at,at+1,bad+i,1));source_edit_probe=0;
        check(!source_edit_writes&&!source_edit_marks&&!memcmp(&s,&before,sizeof(s))&&
              !memcmp(memory[s.page],initial,16000)&&page_versions[s.page]==version&&allocation_calls==allocations);
    }
    memset(too_large,'a',sizeof(too_large));
    source_edit_reset();source_edit_probe=1;
    check(!source_replace(&s,0,0,too_large,sizeof(too_large)));source_edit_probe=0;
    check(!source_edit_reads&&!source_edit_writes&&!source_edit_marks&&!memcmp(&s,&before,sizeof(s)));
    check(!source_replace(&s,2,1,(const uint8_t*)"x",1));
    check(!source_replace(&s,16000,16001,(const uint8_t*)"x",1));
    source_edit_reset();source_edit_probe=1;
    check(source_replace(&s,8000,8000,0,0));source_edit_probe=0;
    check(!source_edit_reads&&!source_edit_writes&&!source_edit_marks&&!memcmp(&s,&before,sizeof(s))&&page_versions[s.page]==version);
    /* Even an identical nonempty overwrite is a mutation for cached builds. */
    load("begin end.");check(compile(&s,&b,&d)&&build_ready(&s,&b));
    check(source_replace(&s,0,0,0,0)&&build_ready(&s,&b));
    version=page_versions[s.page];
    check(source_replace(&s,0,1,(const uint8_t*)"b",1));check(!build_ready(&s,&b)&&!build_check(&s,&b));
    check((uint32_t)(page_versions[s.page]-version)==1);
    build_close(&b);check(pages_used()==1);
}
static void memory_overlays(void) {
    const char *cases[]={
        "program Test;\noverlay procedure One; begin end; begin One end.",
        "overlay function One:integer; begin One:=1 end; begin writeln(One) end.",
        "procedure Outer;\noverlay procedure Inner; begin end; begin Inner end; begin Outer end."};
    unsigned i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        invalid(cases[i],77);
        check(d.start==(uint16_t)(strstr(cases[i],"overlay")-cases[i]));
        check(!b.valid&&b.pages[0]==255&&b.pages[1]==255&&b.pages[2]==255&&b.map_page==255);
    }
    {
        HostSource include={"H:/OVER.INC","{part}\noverlay procedure One; begin end;"};
        CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};
        disk_sources=&include;disk_source_count=1;
        for(i=0;i<3;++i) {
            options.kind=i==2?COMPILE_CHAIN:COMPILE_PROGRAM;
            load("program Include;\n{$I OVER.INC}\nbegin end.");
            check(!compile_project(&s,&b,&d,0,i?&options:0));
            check(d.code==(i?12:77)&&d.start==7&&!strcmp(cc.name,include.path));
            check(!b.valid&&pages_used()==1);
        }
        disk_sources=0;disk_source_count=0;
    }
}
static void compile_cancellation(void) {
    Source before;uint8_t content[128],stage,result,hash[32];unsigned calls;
    load("{unsaved}\nprogram Cancel;\nvar n:integer;\nbegin n:=7;writeln(n) end.\n");
    s.cursor=9;s.dirty=1;before=s;
    memcpy(content,memory[s.page],s.length);
    for(stage=1;stage<=4;++stage)for(result=1;result<=2;++result) {
        cancel_stage=stage;cancel_result=result;poll_calls=0;
        check(!compile_project(&s,&b,&d,1,0));
        check(d.code==COMPILE_INTERRUPTED&&compile_interrupt_reason()==result);
        check(poll_calls>0&&!cc.polling&&!b.valid&&pages_used()==1);
        if(stage==4&&result==1)printf("Compile cancellation seal checkpoint: %u\n",poll_calls);
        if(stage==3&&result==1)printf("Compile cancellation parser checkpoint: %u\n",poll_calls);
        check(!memcmp(&s,&before,sizeof(s))&&!memcmp(content,memory[s.page],s.length));
        check(b.pages[0]==255&&b.pages[1]==255&&b.pages[2]==255&&b.map_page==255&&b.sources.page==255);
        cancel_stage=0;
        check(compile_project(&s,&b,&d,1,0));check(!cc.polling&&compile_interrupt_reason()==0);
        calls=poll_calls;
        /* Completed-build verification must never use the expired job's
         * diagnostic or consume input, even when the next poll would abort. */
        cancel_stage=1;test_image_hash(&b,hash);
        check(build_check(&s,&b)&&poll_calls==calls);
        build_close(&b);check(pages_used()==1);
    }
    calls=poll_calls;
    check(compile(&s,&b,&d)&&poll_calls==calls);build_close(&b);
    cancel_stage=0;
    {
        char text[4096];unsigned i;
        strcpy(text,"{\n");
        for(i=0;i<100;++i)strcat(text,"a long comment must remain cancellable\n");
        strcat(text,"}\nbegin end.\n");load(text);before=s;
        cancel_stage=5;cancel_result=1;
        check(!compile_project(&s,&b,&d,1,0)&&d.code==COMPILE_INTERRUPTED);
        check(cc.scan>512&&cc.scan<1024&&!memcmp(&s,&before,sizeof(s)));
        check(!b.valid&&!cc.polling&&pages_used()==1);
        cancel_stage=0;check(compile(&s,&b,&d));build_close(&b);
    }
}
/* Nonzero prior-owner bytes, full write coverage, tail/gap zeroing, and
 * interruption before initialization completes must all preserve ownership. */
static void runtime_initialization(void) {
    const char *text="program Init;var pad:array[1..193]of byte;n:integer;const k:integer=17;begin n:=k;writeln(n)end.";
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};Source before;
    uint8_t contents[256],mode,reason;unsigned before_allocations,required,i;
    check(cc_runtime_size()>256&&cc_runtime_size()<=PAGE_SIZE&&runtime_start[0]!=0);
    initialization_probe=1;
    for(mode=0;mode<3;++mode) {
        load(text);before=s;memcpy(contents,memory[s.page],s.length);
        options.kind=mode==2?COMPILE_CHAIN:COMPILE_PROGRAM;
        initialization_reset();before_allocations=allocation_calls;
        check(compile_project(&s,&b,&d,1,mode?&options:0));required=allocation_calls-before_allocations;
        check(copy_started&&copy_complete&&initialized[b.pages[0]]&&build_check(&s,&b));
        printf("PERF01 initialization mode%u: runtime=%u image_stores=%lu allocations=%u\n",mode,cc_runtime_size(),image_stores,required);
        printf("PERF13 initialization mode%u: fills=%u bytes=%u polls=%u sequence=",mode,image_fill_calls,image_fill_bytes,initialization_polls);
        for(i=0;i<initialization_polls;++i)printf("%lu,",(unsigned long)initialization_poll_stores[i]);puts("");
        check(image_fill_calls==129&&image_fill_bytes==3u*PAGE_SIZE-cc_runtime_size());
        check(initialization_polls==(mode==2?261:250));
        for(i=0;i<image_fill_calls;++i) {
            uint8_t index=i==0?0:i<=64?2:1;
            check(image_fill_indexes[i]==index&&
                  image_fill_offsets[i]==(i==0?cc_runtime_size():(i-(index==2?1:65))*256u)&&
                  image_fill_counts[i]==(i==0?PAGE_SIZE-cc_runtime_size():256u));
        }
        build_close(&b);check(pages_used()==1);
        for(i=0;i<required;++i) {
            initialization_reset();fail_after=(int)i;
            check(!compile_project(&s,&b,&d,1,mode?&options:0)&&d.code==99&&!b.valid);
            fail_after=-1;check(pages_used()==1&&!memcmp(&s,&before,sizeof(s))&&!memcmp(contents,memory[s.page],s.length));
        }
        {
            static const unsigned failures[]={0,1,2,32,64,65,66,96,128};
            static const unsigned cancel_after[]={1,33,64,65,97,128};
            for(i=0;i<sizeof(failures)/sizeof(failures[0]);++i) {
                initialization_reset();image_fill_fail_after=(int)failures[i];
                check(!compile_project(&s,&b,&d,1,mode?&options:0)&&d.code==99&&!b.valid);
                image_fill_fail_after=-1;
                check(image_fill_calls==failures[i]+1&&!initialized[image_fill_failed_page]);
                check(!memcmp(memory[image_fill_failed_page]+image_fill_failed_offset,image_fill_failed_bytes,image_fill_failed_count));
                {
                    uint16_t at;uint8_t poisoned=1;
                    for(at=0;at<image_fill_failed_count;++at)if(image_fill_failed_bytes[at]!=0xa5)poisoned=0;
                    check(poisoned&&page_versions[image_fill_failed_page]==image_fill_failed_version);
                }
                check(pages_used()==1&&!cc.polling&&b.pages[0]==255&&b.pages[1]==255&&b.pages[2]==255&&b.map_page==255&&b.sources.page==255);
                check(!memcmp(&s,&before,sizeof(s))&&!memcmp(contents,memory[s.page],s.length));
            }
            for(i=0;i<sizeof(cancel_after)/sizeof(cancel_after[0]);++i)for(reason=1;reason<=2;++reason) {
                initialization_reset();cancel_stage=9;cancel_result=reason;
                image_fill_cancel_after=cancel_after[i];image_fill_cancel_index=i<3?2:1;
                check(!compile_project(&s,&b,&d,1,mode?&options:0));cancel_stage=0;
                check(d.code==COMPILE_INTERRUPTED&&compile_interrupt_reason()==reason&&image_fill_calls==cancel_after[i]);
                check(!initialized[image_fill_cancel_page]&&!cc.polling&&!b.valid&&pages_used()==1);
                check(!memcmp(&s,&before,sizeof(s))&&!memcmp(contents,memory[s.page],s.length));
            }
        }
        for(reason=1;reason<=2;++reason) {
            initialization_reset();cancel_stage=8;cancel_result=reason;
            check(!compile_project(&s,&b,&d,1,mode?&options:0));
            check(d.code==COMPILE_INTERRUPTED&&compile_interrupt_reason()==reason&&copy_started&&!copy_complete);
            check(!cc.polling&&!b.valid&&pages_used()==1);
            check(b.pages[0]==255&&b.pages[1]==255&&b.pages[2]==255&&b.map_page==255&&b.sources.page==255);
            check(!memcmp(&s,&before,sizeof(s))&&!memcmp(contents,memory[s.page],s.length));
            cancel_stage=0;
        }
    }
    initialization_probe=0;
}

static void compilation_options(void) {
    const uint16_t starts[]={0,0x8000,0x8202,0x9000,0xbfff,0xc000,0xc001,0xd000,0xef00,0xf400,0xffff};
    const uint16_t ends[]={0,0xcffe,0xcfff,0xd000,0xe123,0xfbfe,0xfbff,0xfc00,0xffff};
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};CompileLimits limits;
    uint8_t identity[32],hash[32];uint16_t bytes,globals,i,j,at,typed;
    uint32_t start,heap;uint8_t expected,ok,gap_zero;
    load("program Layout;\nvar pad:array[1..513]of byte;marker:integer;\n"
         "const typed:integer=17;\nprocedure p;begin pad[1]:=7 end;\n"
         "begin p;marker:=typed;writeln(marker,pad[1])end.\n");
    check(compile(&s,&b,&d));bytes=b.code_bytes;globals=b.globals_bytes;
    check(globals==515);test_image_hash(&b,identity);build_close(&b);
    for(i=0;i<sizeof(starts)/sizeof(starts[0]);++i)for(j=0;j<sizeof(ends)/sizeof(ends[0]);++j) {
        options.start=starts[i];options.end=ends[j];
        start=options.start?options.start:GLOBAL_BASE+globals;
        heap=(start+bytes+1)&~1UL;
        expected=options.end>=0xcfffu&&options.end<=0xfbffu&&
            start>=(uint32_t)GLOBAL_BASE+globals&&heap+2048u<=(uint32_t)options.end+1;
        check(compile_limits(globals,bytes,&options,&limits)==expected);
        ok=compile_project(&s,&b,&d,0,&options);check(ok==expected);
        if(ok) {
            check(b.code_origin==start&&b.code_bytes==bytes&&b.globals_bytes==globals);
            check(image_get(&b,30)==(uint8_t)options.end&&image_get(&b,31)==(uint8_t)(options.end>>8));
            check(b.length==start-0x4000u+bytes&&build_check(&s,&b));
            gap_zero=1;
            for(at=GLOBAL_BASE+globals-0x4000u;at<start-0x4000u;++at)if(image_get(&b,at))gap_zero=0;
            check(gap_zero);
            typed=0;
            for(at=0;at<cc.symbol_count;++at)if(cc.symbols[at].role==S_TYPED) {
                typed=cc.symbols[at].address+b.code_origin-CC_CODE_BASE-0x4000u;
                check(image_get(&b,typed)==17&&image_get(&b,typed+1)==0);
            }
            check(typed!=0);
            test_image_hash(&b,hash);check((!memcmp(identity,hash,32))==(start==GLOBAL_BASE+globals&&options.end==0xfbffu));
            build_close(&b);
        } else check(d.code==99&&!b.valid);
        check(pages_used()==1);
    }
    options.start=0xd001;options.end=(uint16_t)(((0xd001UL+bytes+1)&~1UL)+2048u-1);
    check(compile_project(&s,&b,&d,0,&options));build_close(&b);
    --options.end;check(!compile_project(&s,&b,&d,0,&options)&&d.code==99&&pages_used()==1);
    options.start=0;options.end=0xfbff;
    check(!compile_limits(0xffff,1,&options,&limits));
    check(!compile_limits(0,0xffff,&options,&limits));
}
/* Execute only the generated activation instruction subset. Ordinary globals
 * are caller-owned sentinels; all other memory starts as an earlier program's
 * unrelated bytes. Native execution independently checks the same handoff. */
static void chain_cases(void) {
    static uint8_t caller[65536],writes[65536];
    const char *programs[]={"{$C-}var shared:integer;const value:integer=17;begin shared:=value end.",
        "{$C-}var shared:integer;begin if ConOutPtr=0 then halt;writeln(shared)end.",
        ("{$C-}var shared:integer;begin if ConStPtr=0 then halt;if ConOutPtr=0 then halt;\n"
        "if LstOutPtr=0 then halt;if AuxOutPtr=0 then halt;if UsrOutPtr=0 then halt;writeln(shared)end.")};
    const uint16_t origins[]={0,0x9000,0xc001,0xd100};
    CompileOptions options={0,0xfbffu,COMPILE_CHAIN};
    NativeImage identity;uint8_t header[160],a=0,op,expected[3],zero;
    uint16_t i,j,k,ip,hl=0,address,target,sites[5],first;unsigned scenario;
    for(scenario=0;scenario<3;++scenario)for(k=0;k<(scenario?1:4);++k) {
        load(programs[scenario]);options.start=origins[k];
        check(compile_project(&s,&b,&d,0,&options));check(build_is_chain(&b));
        check(b.length==96+b.code_bytes&&b.globals_bytes>=2&&build_check(&s,&b));
        for(i=0;i<96;++i)header[i]=image_get(&b,i);
        check(native_header(header,1,&identity)==0&&identity.origin==b.code_origin&&identity.bytes==b.code_bytes);
        check(identity.length==b.length);
        memset(caller,0xa5,sizeof(caller));memset(writes,0,sizeof(writes));
        for(i=0;i<b.code_bytes;++i)caller[b.code_origin+i]=image_get(&b,96+i);
        ip=b.code_origin;first=ip;
        while(ip<first+CC_ACTIVATION_BYTES) {
            op=caller[ip++];
            if(op==0)continue;
            if(op==0x3e){a=caller[ip++];continue;}
            if(op==0x21){hl=caller[ip]|((uint16_t)caller[ip+1]<<8);ip+=2;continue;}
            if(op==0xc3){check(ip==first+CHAIN_ENTRY_BYTES-2);target=caller[ip]|((uint16_t)caller[ip+1]<<8);check(target==first+CC_ACTIVATION_BYTES);ip=target;break;}
            check(op==0x32||op==0x22);address=caller[ip]|((uint16_t)caller[ip+1]<<8);ip+=2;
            check(address<GLOBAL_BASE||address>=GLOBAL_BASE+2);
            caller[address]=op==0x32?a:(uint8_t)hl;writes[address]=1;
            if(op==0x22){caller[address+1]=(uint8_t)(hl>>8);writes[address+1]=1;}
        }
        check(ip==first+CC_ACTIVATION_BYTES&&caller[GLOBAL_BASE]==0xa5&&caller[GLOBAL_BASE+1]==0xa5);
        for(i=0;i<CHAIN_PROFILE_BYTES;++i)check(caller[first+CHAIN_ENTRY_BYTES+i]==cc_runtime_profile_get(i)&&!writes[first+CHAIN_ENTRY_BYTES+i]);
        sites[0]=cc_runtime_private(RP_INPUT_KEY);sites[1]=cc_runtime_get(RT_CHAR-0x4000u+1)|((uint16_t)cc_runtime_get(RT_CHAR-0x4000u+2)<<8);
        sites[2]=cc_runtime_private(RP_INPUT_ECHO);sites[3]=cc_runtime_private(RP_INPUT_OPEN);sites[4]=cc_runtime_private(RP_NORMAL_ERROR);
        for(i=0;i<5;++i) {
            address=sites[i];target=i==0?cc.keyboard_entry:i==1?cc.hook_character:i==2?cc.hook_echo:i==3?cc.hook_open:cc.error_entry;
            if(target){target+=b.code_origin-CC_CODE_BASE;expected[0]=0xc3;expected[1]=(uint8_t)target;expected[2]=(uint8_t)(target>>8);}
            else for(j=0;j<3;++j)expected[j]=cc_runtime_get(address-0x4000u+j);
            for(j=0;j<3;++j)check(caller[address+j]==expected[j]&&writes[address+j]);
        }
        check(caller[RT_CBREAK]==0&&writes[RT_CBREAK]);
        for(i=0;i<HOOK_COUNT;++i)if(cc.hook_variables[i]) {
            address=cc.hook_variables[i];target=cc.hook_defaults[i]+b.code_origin-CC_CODE_BASE;
            check(caller[address]==(uint8_t)target&&caller[address+1]==(uint8_t)(target>>8));
        }
        for(i=0;i<cc.symbol_count;++i)if(cc.symbols[i].role==S_TYPED) {
            address=96+cc.symbols[i].address-CC_CODE_BASE;
            check(image_get(&b,address)==17&&image_get(&b,address+1)==0);
        }
        zero=image_get(&b,97);far_set(b.pages[0],97,zero^1);check(!build_check(&s,&b));
        far_set(b.pages[0],97,zero);check(!build_check(&s,&b));
        build_close(&b);check(pages_used()==1);
    }
    options.kind=2;load("begin end.");check(!compile_project(&s,&b,&d,0,&options)&&d.code==99&&!b.valid&&pages_used()==1);
}

/* Compare every embedded byte, rebasing only the caller's declared operands.
 * Module/gate bytes are private fixed-address assets, never Pascal relocations. */
static void chain_call_cases(void) {
    CompileOptions options={0x8100,0xfbffu,COMPILE_PROGRAM};
    uint16_t i,j,entry,module,offset,actual,expected,at,calls,one_bytes=0;
    uint8_t kind,repeated,expected_byte;
    for(kind=0;kind<2;++kind)for(repeated=0;repeated<2;++repeated) {
        options.kind=kind?COMPILE_CHAIN:COMPILE_PROGRAM;
        options.start=kind?0x8200:0x8100;
        load(repeated?"var f:file;begin Chain(f);Chain(f)end.":"var f:file;begin Chain(f)end.");
        check(compile_project(&s,&b,&d,0,&options));
        check(cc.transfer_entry&&cc.file_entry&&cc.temporaries==0&&build_check(&s,&b));
        at=kind?20:36;expected_byte=image_get(&b,at);
        far_set(b.pages[0],at,expected_byte^1);check(!build_check(&s,&b));
        far_set(b.pages[0],at,expected_byte);check(!build_check(&s,&b));b.versions[0]=page_versions[b.pages[0]];
        expected_byte=far_get(b.map_page,20);
        far_set(b.map_page,20,expected_byte^1);check(!build_check(&s,&b));
        far_set(b.map_page,20,expected_byte);check(!build_check(&s,&b));b.versions[3]=page_versions[b.map_page];
        entry=cc.transfer_entry;module=entry-TRANSFER_MODULE_BYTES-TRANSFER_GATE_BYTES;
        offset=kind?96:b.code_origin-0x4000;
        for(i=0;i<TRANSFER_MODULE_BYTES+TRANSFER_GATE_BYTES;++i) {
            expected_byte=i<TRANSFER_MODULE_BYTES?cc_transfer_asset_get(0,i):
                cc_transfer_asset_get(1,i-TRANSFER_MODULE_BYTES);
            check(image_get(&b,offset+module-CC_CODE_BASE+i)==expected_byte);
        }
        actual=entry+b.code_origin-CC_CODE_BASE;
        calls=0;
        for(i=0;i+6<=b.code_bytes;++i)
            if(image_get(&b,offset+i)==0xcd&&image_get(&b,offset+i+1)==(uint8_t)RT_IOCALL&&
               image_get(&b,offset+i+2)==(uint8_t)(RT_IOCALL>>8)&&
               image_get(&b,offset+i+3)==(uint8_t)actual&&image_get(&b,offset+i+4)==(uint8_t)(actual>>8)&&
               image_get(&b,offset+i+5)==1)++calls;
        check(calls==(repeated?2:1));
        for(i=0;i<TRANSFER_CALL_BYTES;++i) {
            expected_byte=cc_transfer_asset_get(2,i);
            for(j=0;j<TRANSFER_CALL_RELOC_COUNT+4;++j) {
                if(j<TRANSFER_CALL_RELOC_COUNT) {
                    at=TRANSFER_CALL_RELOC_OFFSET+2*j;
                    at=(cc_transfer_asset_get(2,at)|((uint16_t)cc_transfer_asset_get(2,at+1)<<8))-CC_CODE_BASE;
                    expected=cc_transfer_asset_get(2,at)|((uint16_t)cc_transfer_asset_get(2,at+1)<<8);
                    expected+=actual-CC_CODE_BASE;
                } else if(j==TRANSFER_CALL_RELOC_COUNT) {
                    at=TRANSFER_CALL_MODULE_OPERAND;expected=module+b.code_origin-CC_CODE_BASE;
                } else if(j==TRANSFER_CALL_RELOC_COUNT+1) {
                    at=TRANSFER_CALL_GATE_OPERAND;expected=actual-TRANSFER_GATE_BYTES;
                } else if(j==TRANSFER_CALL_RELOC_COUNT+2) {
                    at=TRANSFER_CALL_FILE_OPERAND;expected=cc.file_entry+b.code_origin-CC_CODE_BASE;
                } else {
                    at=TRANSFER_CALL_UNPACK_OPERAND;expected=cc.unpack_entry+b.code_origin-CC_CODE_BASE;
                }
                if(i==at)expected_byte=(uint8_t)expected;
                if(i==at+1)expected_byte=(uint8_t)(expected>>8);
            }
            check(image_get(&b,offset+entry-CC_CODE_BASE+i)==expected_byte);
        }
        if(!repeated)one_bytes=b.code_bytes;
        else check(b.code_bytes>one_bytes&&b.code_bytes-one_bytes<256);
        build_close(&b);check(pages_used()==1);
    }
    /* Pure disk transfer programs need no logical Text adapters. A Text
     * type first encountered in a later routine must still resolve the one
     * file engine that an earlier routine already requested. */
    load("var f:file;begin Execute(f)end.");
    check(compile(&s,&b,&d)&&cc.file_entry&&cc.transfer_entry&&cc.text_type==255);
    at=b.code_origin-CC_CODE_BASE+cc.file_stream_operand-0x4000;
    check(!image_get(&b,at)&&!image_get(&b,at+1)&&!image_get(&b,at+2)&&!cc.keyboard_entry);
    build_close(&b);
    load("var f:file;procedure first;begin Assign(f,'H:/first')end;\n"
         "procedure later;var t:Text;begin Assign(t,'H:/later');Rewrite(t);Writeln(t,42);Close(t)end;\n"
         "begin first;later end.");
    check(compile(&s,&b,&d)&&cc.file_entry&&cc.text_type!=255&&cc.keyboard_entry);
    at=b.code_origin-CC_CODE_BASE+cc.file_stream_operand-0x4000;
    check(image_get(&b,at)==0xcd);build_close(&b);
    load("var f:file;procedure Execute(n:integer);begin end;begin Execute(1);Chain(f)end.");
    check(compile(&s,&b,&d)&&cc.transfer_entry!=0);build_close(&b);
    load("var f:file;begin {$I-}Chain(f);Execute(f)end.");
    check(compile(&s,&b,&d)&&cc.transfer_entry!=0);build_close(&b);
    load("procedure Chain(n:integer);begin end;begin Chain(1)end.");
    check(compile(&s,&b,&d)&&!cc.transfer_entry&&!cc.file_entry);build_close(&b);
    load("procedure Execute(n:integer);begin end;begin Execute(1)end.");
    check(compile(&s,&b,&d)&&!cc.transfer_entry&&!cc.file_entry);build_close(&b);
    invalid("var n:integer;begin Chain(n)end.",21);
    invalid("begin Chain('NEXT.CHN')end.",21);
    for(kind=0;kind<10;++kind) {
        static const char * const names[]={"chain-call","chain-next","chain-cleanup",
            "execute-call","execute-target","execute-retry","execute-chain","execute-chain-next",
            "execute-failure","execute-checked"};
        char text[4096];size_t length;
        char path[128];FILE *file;
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[kind]);file=fopen(path,"rb");
        check(file!=NULL);length=fread(text,1,sizeof(text),file);
        check(feof(file)&&!ferror(file)&&fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        options.kind=kind==1||kind==7?COMPILE_CHAIN:COMPILE_PROGRAM;
        options.start=kind<3||kind==4?0x8040:0x8080;
        if(!compile_project(&s,&b,&d,0,&options)) {
            fprintf(stderr,"transfer fixture %s\n",path);check(0);
        }
        build_close(&b);
    }
}

static void startup_cases(void) {
    extern const uint8_t transfer_start[];
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};
    uint16_t bytes,entry,at,i;uint8_t header[160];NativeImage identity;
    static const uint16_t entries[]={0xbfff,0xc000,0xc001,0xf100};
    load("var s:string[80];begin s:='entry' end.");
    check(compile(&s,&b,&d));bytes=b.code_bytes;build_close(&b);
    for(at=0;at<sizeof(entries)/sizeof(*entries);++at) {
        options.start=entries[at]-(bytes-EXECUTE_ENTRY_BYTES);
        check(compile_project(&s,&b,&d,0,&options)&&b.code_bytes==bytes&&build_check(&s,&b));
        for(i=0;i<160;++i)header[i]=image_get(&b,i);
        entry=header[12]|((uint16_t)header[13]<<8);
        check(entry==entries[at]&&header[1]==header[12]&&header[2]==header[13]);
        check((header[14]|((uint16_t)header[15]<<8))==0x40a3);
        check(native_header(header,0,&identity)==0&&identity.origin==options.start);
        for(i=0;i<EXECUTE_ENTRY_BYTES;++i)
            check(image_get(&b,entry-0x4000u+i)==transfer_start[TRANSFER_MODULE_BYTES+TRANSFER_GATE_BYTES+TRANSFER_CALL_BYTES+i]);
        build_close(&b);check(pages_used()==1);
    }
    options.start=0;options.kind=COMPILE_CHAIN;
    check(compile_project(&s,&b,&d,0,&options)&&build_check(&s,&b));
    check(b.code_bytes==bytes-EXECUTE_ENTRY_BYTES+CC_ACTIVATION_BYTES);
    build_close(&b);check(pages_used()==1);
}

static void keyboard_corpus(void) {
    const char *names[]={"kbdread","kbdbind","kbdlife","kbdupper","kbdfail","conread","conbind","conoutput","conupper","conoutfail","confail","trmread","trmfail","devoutput","devfail"};
    char path[80],text[16384];
    unsigned i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        FILE *file;
        size_t length;
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);
        file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d));check(cc.file_entry&&cc.temporaries==0);build_close(&b);
    }
}
/* Keep the regression's classifier at the placement where its old inline
 * four-byte result would cross BFFF/C000. Native execution checks the actual
 * output and neighboring data; this only establishes that layout precondition. */
static void console_elision(void) {
    static const char *const dynamic[]={
        "{$C-}begin cbreak:=true;writeln('x')end.",
        "{$C-}var flag:byte absolute $4152;begin flag:=1;writeln('x')end.",
        "{$C-}begin mem[$4152]:=1;writeln('x')end.",
        "{$C-}type p=^byte;var q:p;begin q:=ptr($4152);q^:=1;writeln('x')end.",
        "{$C-}begin inline($00);writeln('x')end.",
        "{$C-}procedure p;external $1234;begin p;writeln('x')end.",
        "{$C-}begin conoutptr:=0;writeln('x')end."
    };
    uint16_t disabled,enabled;unsigned i;
    load("{$C-}begin writeln('x')end.");check(compile(&s,&b,&d));
    disabled=b.code_bytes;check(!cc.keyboard_entry&&!cc.hook_character&&!cc.hook_echo);build_close(&b);
    load("begin writeln('x')end.");check(compile(&s,&b,&d));
    enabled=b.code_bytes;check(cc.keyboard_entry&&enabled>disabled+258);build_close(&b);
    for(i=0;i<sizeof(dynamic)/sizeof(*dynamic);++i) {
        load(dynamic[i]);check(compile(&s,&b,&d));
        check(cc.console_dynamic&&cc.keyboard_entry);build_close(&b);
    }
    printf("TP3 console fixed-False=%u enabled=%u saved=%u bytes\n",disabled,enabled,enabled-disabled);
}
static void console_boundary(void) {
    char text[2048];FILE *file=fopen("tests/native/ctlboundary.pas","rb");
    uint16_t at,entry=0,area=cc_runtime_private(RP_INPUT_CONSOLE_AREA);
    size_t length;
    check(file!=NULL);length=fread(text,1,sizeof(text),file);
    check(feof(file)&&!ferror(file));check(fclose(file)==0);
    check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
    check(compile(&s,&b,&d));
    for(at=b.code_origin-0x4000u;at+8<b.length;++at)
        if(image_get(&b,at)==0x11&&image_get(&b,at+1)==(uint8_t)area&&
           image_get(&b,at+2)==(uint8_t)(area>>8)&&image_get(&b,at+3)==0x0e&&
           image_get(&b,at+4)==1&&image_get(&b,at+5)==0x2a&&
           image_get(&b,at+6)==(uint8_t)RT_PC&&image_get(&b,at+7)==(uint8_t)(RT_PC>>8)) {
            check(!entry);entry=at+0x4000u-7;
        }
    if(entry!=0xbffeu)fprintf(stderr,"console classifier at %04x; adjust fixture padding by %d\n",entry,0xbffe - (int)entry);
    check(entry==0xbffeu);check((area&0x3fffu)<=0x3ffcu);
    build_close(&b);check(pages_used()==1);
}
static void interruption_corpus(void) {
    const char *names[]={"keyuraw","keyuabi","keyuframe","keyuqueue","keyucached","keyurep","keyuwhile","keyufor","keyugoto","keyufail","keyuignore"};
    char path[80],text[4096];unsigned i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        FILE *file;size_t length;
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);
        file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d)&&cc.temporaries==0);
        check((cc.interrupt_entry!=0)==(i!=0));
        build_close(&b);check(pages_used()==1);
    }
    invalid("{$U}begin end.",93);invalid("{$U1}begin end.",93);
    invalid("{$U+ ,R+}begin end.",93);
    load("{$C-}begin end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==0);build_close(&b);
    load("{$C+}{$C-}begin end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==0);build_close(&b);
    load("begin {$C-}end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    load("var n:integer;{$C-}begin end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    load("{$C+}procedure p;{$C-}begin end;begin p end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    invalid("{$C}begin end.",93);invalid("{$C1}begin end.",93);
}
static void bridge_corpus(void) {
    const char *names[]={"bridgeframe","bridgeargs","bridgereloc","bridgefile","bridgeerror","bridgeguard"};
    char path[80],text[16384];
    unsigned i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        FILE *file; size_t length;
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);
        file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d));check(cc.temporaries==0);
        if(!strcmp(names[i],"bridgereloc")) check(b.globals_bytes==3&&b.code_origin==0x8003);
        else check(b.globals_bytes==0&&image_get(&b,0x4003)==3&&image_get(&b,0x4004)==195);
        build_close(&b);
    }
}
static void hook_corpus(void) {
    const char *names[]={"hookout","hookmulti","hookaux","hookstatus","hookusr","hookecho","hookpure","hookerror","hookioerr"};
    char path[80],text[16384]; unsigned i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        FILE *file; size_t length;
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d));check(cc.temporaries==0);build_close(&b);
    }
}
static uint16_t map_word(uint16_t at) { return far_get(b.map_page,at)|((uint16_t)far_get(b.map_page,at+1)<<8); }
static void ordered_map(void) {
    uint16_t at=96+map_word(84),last=0,begin,end;
    for(;at<b.map_length;at+=16) {
        begin=map_word(at); end=map_word(at+2);
        check(begin>=last&&begin<end&&end<=b.code_bytes);
        check(map_word(at+8)<=map_word(at+12)&&map_word(at+12)<=s.length);
        last=end;
    }
}
static unsigned calls(uint16_t entry,int mode) {
    uint16_t i,at=b.code_origin-0x4000u;
    unsigned count=0;
    for(i=0;i+5<b.code_bytes;++i) if(image_get(&b,at+i)==0xcd&&
        image_get(&b,at+i+1)==(uint8_t)entry&&image_get(&b,at+i+2)==(uint8_t)(entry>>8)&&
        (mode<0||image_get(&b,at+i+5)==mode)) ++count;
    return count;
}
static void index_modes(void) {
    static const uint16_t strides[]={0,1,2,3,4,5,6,7,8,15,16,17,31,32,63,64,
        127,128,129,254,255,256,257,511,512,1023,1024,2047,4095,4096,8191,8192,16383};
    char text[256],element[80];
    uint16_t bytes,i;
    uint8_t mode;
    for(i=0;i<sizeof(strides)/sizeof(strides[0]);++i) {
        if(strides[i])snprintf(element,sizeof(element),"array[1..%u]of byte",strides[i]);
        else strcpy(element,"record end");
        bytes=0;
        for(mode=0;mode<2;++mode) {
            snprintf(text,sizeof(text),"{$X%c}type item=%s;var a:array[0..0]of item absolute $9000;i,p:integer;begin p:=addr(a[i])end.",mode?'-':'+',element);
            load(text);check(compile(&s,&b,&d));check(cc.index_speed==!mode);
            check(cc.temporaries==0);
            if(!mode){check(calls(RT_MUL,-1)==0);bytes=b.code_bytes;}
            else {check(b.code_bytes<=bytes);if(strides[i]==255)check(calls(RT_MUL,-1)==1);}
            build_close(&b);
        }
    }
    load("(*$x-*)type item=array[1..255]of byte;var a:array[1..2]of item;i,p:integer;begin p:=addr(a[i]);{$R+,X+}p:=addr(a[i])end.");
    check(compile(&s,&b,&d));check(calls(RT_MUL,-1)==1);check(calls(RT_INDEX,-1)==1);build_close(&b);
    load("begin end.");check(compile(&s,&b,&d));check(cc.index_speed==1);build_close(&b);
    invalid("{$X}begin end.",93);invalid("{$X*}begin end.",93);
    {
        const char *names[]={"indexmodes","indexwrap","indexrange","indexrsmall"};
        char source[4096],path[80];
        FILE *fixture;size_t length;
        for(i=0;i<4;++i) {
            snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);fixture=fopen(path,"rb");check(fixture!=NULL);
            length=fread(source,1,sizeof(source),fixture);check(feof(fixture)&&!ferror(fixture));check(fclose(fixture)==0);
            check(source_import(&s,(const uint8_t*)source,(uint16_t)length,&d));
            check(compile(&s,&b,&d));check(cc.temporaries==0);build_close(&b);
        }
    }
}
static void source_catalog_cases(void) {
    Sources set,stale,next;Source origin,snapshot,view,other;
    uint8_t before=pages_used(),old,expected[32];
    check(source_new(&origin,"H:/origin.pas"));
    check(source_import(&origin,(const uint8_t*)"begin end.",10,&d));
    check(sources_new(&set));check(source_snapshot(&origin,&snapshot));
    check(sources_take(&set,&snapshot,&origin)==0);check(set.count==1);
    check(sources_ready(&set,0)&&sources_resident(&set,0));
    check(sources_find(&set,"H:/origin.pas")==0&&sources_find(&set,"H:/ORIGIN.PAS")==-1);
    check(sources_get(&set,0,&view)&&view.page==snapshot.page&&view.length==10);
    test_source_hash(&origin,expected);check(sources_get(&set,0,&view)&&view.length==snapshot.length);
    check(sources_byte(&set,0,0)=='b'&&sources_byte(&set,0,10)==0);
    old=far_get(origin.page,0);far_set(origin.page,0,'B');check(!sources_ready(&set,0));
    far_set(origin.page,0,old);check(!sources_ready(&set,0));
    check(sources_take(&set,&snapshot,&origin)==0&&sources_ready(&set,0));
    fail_drop=snapshot.page;check(!sources_remove(&set,0)&&set.count==1);
    check(!sources_close(&set)&&set.count==1&&page_role(snapshot.page)==PAGE_SNAPSHOT);fail_drop=-1;
    check(source_new(&other,"H:/second.pas"));
    check(source_import(&other,(const uint8_t*)"var n:integer;",14,&d));
    check(sources_take(&set,&other,&other)==1);
    strcpy(view.path,"H:/second.pas");fail_drop=other.page;
    check(sources_take(&set,&view,&origin)<0&&set.count==2&&sources_ready(&set,1));
    fail_drop=-1;check(sources_take(&set,&view,&origin)==0&&set.count==1);
    check(!page_role(other.page)&&sources_find(&set,"H:/origin.pas")==-1);
    strcpy(view.path,"H:/renamed.pas");check(sources_take(&set,&view,&origin)==0&&set.count==1);
    check(sources_find(&set,"H:/origin.pas")==-1&&sources_find(&set,"H:/renamed.pas")==0);
    check(source_new(&other,"H:/second.pas"));check(sources_take(&set,&other,&other)==1);
    view=other;strcpy(view.path,"H:/renamed.pas");fail_drop=snapshot.page;
    check(sources_take(&set,&view,&view)<0&&set.count==2&&sources_ready(&set,0));
    fail_drop=-1;check(sources_take(&set,&view,&view)==0&&set.count==1);
    check(sources_get(&set,0,&view)&&view.page==other.page&&!page_role(snapshot.page));
    stale=set;check(sources_close(&set)&&set.page==255);check(!sources_valid(&stale));
    check(sources_new(&next));check(next.page==stale.page&&!sources_valid(&stale));check(sources_close(&next));
    source_close(&origin);check(pages_used()==before);
}
static void replaced_source_cases(void) {
    Sources set;Source work,view,other;
    uint8_t before=pages_used(),prior,foreign,scenario;
    uint32_t lifetime;
    for(scenario=0;scenario<3;++scenario) {
        check(source_new(&work,"H:/edited.pas"));
        check(source_import(&work,(const uint8_t*)"abc",3,&d));
        check(sources_new(&set)&&sources_take(&set,&work,&work)==0);
        prior=work.page;
        if(scenario==1) check(source_move(&work,0,1,3,0));
        else check(source_insert_from(&work,3,&work,0,1,0));
        check(work.page!=prior&&!page_role(prior)&&!sources_ready(&set,0));
        foreign=255;
        if(scenario==2) {
            foreign=page_new(PAGE_COMPILER);check(foreign==prior);
            lifetime=page_lifetime(foreign);far_set(foreign,0,0xa5);
            check(sources_byte(&set,0,0)==0); /* never expose the new owner */
        }
        check(sources_take(&set,&work,&work)==0&&sources_ready(&set,0));
        check(sources_get(&set,0,&view)&&view.page==work.page&&view.length==work.length);
        check(sources_byte(&set,0,0)==(scenario==1?'b':'a'));
        check(sources_close(&set));
        if(foreign!=255) {
            check(page_role(foreign)==PAGE_COMPILER&&page_lifetime(foreign)==lifetime&&far_get(foreign,0)==0xa5);
            check(page_drop(foreign));
        }
        check(pages_used()==before);
    }
    /* A reused page number cannot rename an unrelated stale catalog record. */
    check(source_new(&work,"H:/stale.pas"));check(sources_new(&set));
    check(sources_take(&set,&work,&work)==0);prior=work.page;source_close(&work);
    check(source_new(&other,"H:/fresh.pas")&&other.page==prior);
    check(sources_take(&set,&other,&other)==1&&set.count==2);
    check(sources_remove(&set,0)&&sources_ready(&set,0));
    check(sources_find(&set,"H:/fresh.pas")==0&&sources_find(&set,"H:/stale.pas")<0);
    check(sources_close(&set)&&pages_used()==before);

    /* Reopening the same path on the same page is a new allocation too. */
    check(source_new(&work,"H:/reopened.pas"));check(sources_new(&set));
    check(sources_take(&set,&work,&work)==0);prior=work.page;source_close(&work);
    check(source_new(&other,"H:/reopened.pas")&&other.page==prior);
    lifetime=page_lifetime(other.page);
    check(sources_take(&set,&other,&other)==0&&set.count==1&&sources_ready(&set,0));
    check(page_lifetime(other.page)==lifetime&&sources_close(&set)&&pages_used()==before);

    /* Packed metadata stays authenticated while a stale include is removed. */
    check(source_new(&work,"H:/packed.pas"));check(source_snapshot(&work,&view));
    check(source_new(&other,"H:/packed.inc"));check(sources_new(&set));
    check(sources_take(&set,&view,&work)==0&&sources_take(&set,&other,0)==1);
    check(sources_pack(&set)&&set.base&&sources_valid(&set));
    prior=other.page;source_close(&other);foreign=page_new(PAGE_RUN);check(foreign==prior);
    lifetime=page_lifetime(foreign);
    check(!sources_ready(&set,1)&&sources_remove(&set,1)&&sources_ready(&set,0));
    check(sources_close(&set)&&page_lifetime(foreign)==lifetime&&page_drop(foreign));
    source_close(&work);check(pages_used()==before);

    /* Removal/close may forget a released record, but never free its reuse. */
    for(scenario=0;scenario<2;++scenario) {
        check(source_new(&work,"H:/removed.pas"));check(sources_new(&set));
        check(sources_take(&set,&work,&work)==0);prior=work.page;source_close(&work);
        foreign=255;
        if(scenario) {foreign=page_new(PAGE_RUN);check(foreign==prior);lifetime=page_lifetime(foreign);}
        check(sources_remove(&set,0)&&set.count==0&&sources_close(&set));
        if(foreign!=255) {check(page_role(foreign)==PAGE_RUN&&page_lifetime(foreign)==lifetime);check(page_drop(foreign));}
        check(pages_used()==before);
    }
    /* Save As also authenticates the destination owner's lifetime. */
    check(source_new(&work,"H:/destination.pas"));check(sources_new(&set));
    check(sources_take(&set,&work,&work)==0);prior=work.page;source_close(&work);
    foreign=page_new(PAGE_RUN);check(foreign==prior);lifetime=page_lifetime(foreign);
    check(source_new(&other,"H:/renamed.pas"));check(sources_take(&set,&other,&other)==1);
    view=other;strcpy(view.path,"H:/destination.pas");
    check(sources_take(&set,&view,&view)==0&&set.count==1&&sources_ready(&set,0));
    check(page_role(foreign)==PAGE_RUN&&page_lifetime(foreign)==lifetime);
    check(sources_close(&set)&&page_drop(foreign)&&pages_used()==before);
}
static void packed_catalog_cases(void) {
    static uint8_t text[PAGE_SIZE];
    const uint16_t lengths[]={0,10,PAGE_SIZE-158,PAGE_SIZE-157,SOURCE_LIMIT};
    Sources set,stale;Source origin,snapshot,include,other,view;
    uint8_t before=pages_used(),metadata,hash[32],expected[32],packed;
    uint16_t i,n;unsigned scenario;
    for(i=0;i<PAGE_SIZE;++i)text[i]=i%80==79?'\n':'x';
    for(scenario=0;scenario<sizeof(lengths)/sizeof(lengths[0]);++scenario) {
        n=lengths[scenario];
        check(source_new(&origin,"H:/root.pas"));
        check(source_import(&origin,text,n,&d)&&origin.length==n);
        check(source_snapshot(&origin,&snapshot)&&snapshot.length==n);
        check(sources_new(&set));check(sources_take(&set,&snapshot,&origin)==0);
        metadata=set.page;stale=set;test_source_hash(&snapshot,expected);
        check(sources_pack(&set));packed=n<PAGE_SIZE-157;
        check((set.base!=0)==packed&&sources_valid(&set)&&sources_ready(&set,0));
        check(pages_used()==before+3-packed);
        check(sources_get(&set,0,&view)&&view.page==snapshot.page&&view.length==n);
        test_source_hash(&view,hash);check(!memcmp(hash,expected,32));
        check(sources_get(&set,0,&view)&&view.length==snapshot.length);
        check(sources_find(&set,"H:/root.pas")==0&&sources_origin(&set,0)==origin.page);
        check(sources_byte(&set,0,n)==0);
        if(packed) {
            check(set.page==snapshot.page&&set.base==n+1&&!page_role(metadata));
            check(!sources_valid(&stale));check(sources_pack(&set));
            fail_drop=snapshot.page;check(!sources_close(&set)&&set.count==1&&sources_valid(&set));
            fail_drop=-1;
            stale=set;far_set(set.page,n?0:set.base,far_get(set.page,n?0:set.base));
            check(!sources_valid(&set)&&!sources_ready(&set,0)&&!sources_close(&set));
            /* Restore the test observer only, to retire the deliberately
             * corrupted owner without weakening production authentication. */
            set.version=page_versions[set.page];
            check(!sources_valid(&stale));
        }
        if(n==PAGE_SIZE-158) {
            check(source_snapshot(&origin,&other));strcpy(other.path,"H:/grow.inc");
            fail_after=0;check(sources_take(&set,&other,0)<0);
            check(set.base&&set.count==1&&sources_ready(&set,0)&&page_role(other.page));
            fail_after=-1;check(sources_take(&set,&other,0)==1&&!set.base);
            check(sources_ready(&set,0)&&sources_ready(&set,1));
            check(sources_pack(&set)&&!set.base); /* Two records cannot fit. */
            check(sources_get(&set,0,&view));test_source_hash(&view,hash);check(!memcmp(hash,expected,32));
        }
        check(sources_close(&set));source_close(&origin);check(pages_used()==before);
    }
    check(source_new(&origin,"H:/root.pas"));
    check(source_import(&origin,(const uint8_t*)"begin end.",10,&d));
    check(source_snapshot(&origin,&snapshot));check(sources_new(&set));
    check(sources_take(&set,&snapshot,&origin)==0);
    check(source_snapshot(&origin,&include));strcpy(include.path,"H:/part.inc");
    check(sources_take(&set,&include,0)==1);
    check(source_snapshot(&origin,&other));strcpy(other.path,"H:/other.inc");
    check(sources_take(&set,&other,0)==2);metadata=set.page;
    fail_drop=metadata;check(!sources_pack(&set));
    check(set.page==metadata&&!set.base&&sources_valid(&set));
    check(sources_ready(&set,0)&&sources_ready(&set,1)&&pages_used()==before+5);
    fail_drop=-1;check(sources_pack(&set)&&pages_used()==before+4);
    check(sources_ready(&set,0)&&sources_ready(&set,1)&&!sources_resident(&set,1));
    check(!sources_remove(&set,0)&&set.count==3);
    check(sources_remove(&set,1)&&set.count==2&&sources_ready(&set,0)&&sources_ready(&set,1));
    check(sources_get(&set,1,&view)&&view.page==other.page&&sources_find(&set,"H:/other.inc")==1);
    fail_drop=other.page;check(!sources_close(&set)&&set.count==2&&sources_ready(&set,0));
    fail_drop=-1;check(sources_remove(&set,1)&&set.count==1&&sources_ready(&set,0));
    check(!page_role(include.page)&&sources_find(&set,"H:/part.inc")==-1);
    fail_after=0;check(sources_take(&set,&snapshot,&origin)<0&&set.base&&sources_ready(&set,0));
    fail_after=-1;check(sources_take(&set,&snapshot,&origin)==0&&!set.base&&sources_ready(&set,0));
    check(sources_pack(&set)&&set.base&&sources_ready(&set,0));
    check(source_snapshot(&origin,&other));fail_drop=snapshot.page;
    check(sources_take(&set,&other,&origin)<0&&!set.base&&sources_ready(&set,0));
    check(page_role(other.page)==PAGE_SNAPSHOT&&page_role(snapshot.page)==PAGE_SNAPSHOT);
    fail_drop=-1;check(sources_take(&set,&other,&origin)==0&&!page_role(snapshot.page));
    check(sources_get(&set,0,&view)&&view.page==other.page&&sources_ready(&set,0));
    check(sources_close(&set));source_close(&origin);check(pages_used()==before);
}
/* Exercise the production lookup independently of parser/symbol limits. The
 * job layout shares its catalog page; direct spill storage grows upward. */
static void reference_cases(void) {
    CompilerSources state;Build image;Diagnostic error;
    uint8_t layout,j,page;uint16_t id,limit,catalog,at;
    unsigned long reads;
    const SourcePosition positions[]={0x12345678UL,0x12345679UL,0x12345778UL,
                                      0x12355678UL,0x13345678UL};
    for(layout=0;layout<3;++layout) {
        memset(&cc,0,sizeof(cc));memset(&state,0,sizeof(state));
        memset(&image,0,sizeof(image));memset(&error,0,sizeof(error));
        cc.sources=&state;cc.image=&image;cc.error=&error;
        page=page_new(PAGE_COMPILER);check(page!=255);
        state.refs_page=page;state.borrow_root=1;
        image.sources.page=layout==2?255:page;
        image.sources.count=layout==1?2:1;
        catalog=layout==2?0:(uint16_t)image.sources.count*157u;
        limit=(PAGE_SIZE-catalog)/10u;
        memset(memory[page],0xa5,PAGE_SIZE);
        reference_probe_page=page;reference_reads=0;reference_probe=1;
        for(id=0;id<limit;++id)check(cc_source_reference(id)==id&&!error.code);
        reads=reference_reads;reference_probe=0;
        check(state.refs==limit&&state.refs_page==page&&!state.refs_owned);
        /* A duplicate still succeeds at capacity; a new key must fail. */
        check(cc_source_reference(limit-1)==limit-1&&!error.code&&state.refs==limit);
        check(cc_source_reference(limit)==0&&error.code==99&&state.refs==limit);
        for(id=0;id<catalog;++id)check(memory[page][id]==0xa5);
        error.code=0;
        for(id=0;id<limit;++id)check(cc_reference_position(id)==id&&!error.code);
        printf("PERF02 %s capacity=%u insertion-reads=%lu\n",
               layout==0?"one-source":layout==1?"two-source":"separate",limit,reads);
        check(page_drop(page)&&pages_used()==1);
    }
    memset(&state,0,sizeof(state));memset(&image,0,sizeof(image));memset(&error,0,sizeof(error));
    cc.sources=&state;cc.image=&image;cc.error=&error;
    page=page_new(PAGE_COMPILER);check(page!=255);state.refs_page=page;image.sources.page=255;
    for(j=0;j<sizeof(positions)/sizeof(positions[0]);++j) {
        check(cc_source_reference(positions[j])==j&&!error.code);
        check(cc_reference_position(j)==positions[j]);
    }
    /* Each of the six insertion-context bytes distinguishes the same offset. */
    for(j=0;j<6;++j) {
        state.parent_scan=j==0?1:j==1?256:0;
        state.parent_flags=j==2;state.parent_limit=j==3;
        cc.absolute_mode=j==4;cc.with_limit=j==5;
        check(cc_source_reference(positions[0])==5+j&&!error.code);
        check(cc_source_reference(positions[0])==5+j&&state.refs==6+j);
    }
    state.parent_scan=0;state.parent_flags=state.parent_limit=0;cc.absolute_mode=cc.with_limit=0;
    check(cc_source_reference(positions[0])==0&&state.refs==11);
    /* Even if a duplicate record exists, lookup retains the first ID. */
    at=state.refs*10u;for(j=0;j<10;++j)far_set(page,at+j,far_get(page,j));++state.refs;
    check(cc_source_reference(positions[0])==0);
    error.code=56;id=state.refs;check(cc_source_reference(9)==0&&error.code==56&&state.refs==id);
    error.code=0;state.refs_page=255;check(cc_source_reference(9)==0&&error.code==99);
    check(page_drop(page)&&pages_used()==1);memset(&cc,0,sizeof(cc));
}
static void source_resume_cases(void) {
    const char *roots[]={""," \n\t\n"};
    HostSource part={"H:/EMPTY.INC",""};CompilerSources state;uint8_t i;
    for(i=0;i<2;++i) {
        load(roots[i]);memset(&cc,0,sizeof(cc));memset(&state,0,sizeof(state));
        cc.input=&s;cc.error=&d;cc.sources=&state;
        cc.hexadecimal=cc.number_overflow=cc.literal_length=1;
        cc_next();check(!d.code&&cc.token==T_EOF&&cc.scan==s.length&&
            cc.token_start==s.length&&cc.token_end==s.length&&
            !cc.hexadecimal&&!cc.number_overflow&&!cc.literal_length);
        cc_next();check(!d.code&&cc.token==T_EOF&&cc.scan==s.length);
        /* An existing error must return before any source/resume work. */
        d.code=56;cc.scan=0;cc.sources=0;
        cc_next();check(d.code==56&&cc.token==T_EOF&&!cc.scan);
        memset(&cc,0,sizeof(cc));
    }
    disk_sources=&part;disk_source_count=1;
    for(i=0;i<2;++i) {
        part.text=i?" \n\t\n":"";
        load("{$I EMPTY.INC}\n{$I EMPTY.INC}\nbegin end.");
        check(compile(&s,&b,&d)&&b.sources.count==2&&build_ready(&s,&b));
        build_close(&b);check(pages_used()==1);
    }
    part.text="{$R+,A-,I-,V-,W0,X-,C-,U+,B-}";
    load("{$R-,A+,I+,V+,W4,X+,C+,U-,B+}\n{$I EMPTY.INC}\nbegin end.");
    check(compile(&s,&b,&d)&&b.sources.count==2&&build_ready(&s,&b));
    check(!cc.range_check&&cc.absolute_mode&&cc.io_check&&cc.var_strict&&
        cc.with_limit==4&&cc.index_speed&&cc.console_control&&!cc.user_interrupt&&!cc.input_fresh);
    build_close(&b);check(pages_used()==1);
    part.text="{";load("{$I EMPTY.INC}\nbegin end.");
    check(!compile(&s,&b,&d)&&d.code==91&&pages_used()==1);
    check(!strcmp(compile_error_path(),part.path)&&d.start==1&&d.end==1);
    disk_sources=0;disk_source_count=0;
}
static void include_cases(void) {
    HostSource files[]={
        {"H:/PART.INC","const greeting='included';number=-1.25;\ntype link=^node;node=record value:integer;next:link end;\nconst table:array[1..2]of integer=(1,2);\nvar n:integer;\nprocedure part;begin n:=table[1];writeln(greeting,number)end;"},
        {"H:/more.inc","procedure more;begin part;n:=n+table[2]end;"},
        {"H:/values.inc","11,22"},
        {"H:/arg.inc","Input"},
        {"H:/bad.inc","const broken=32768;"},
        {"H:/scope.PAS","{$A-,I-,R+,V-,W0,X-,B-,U+}const local=1;"},
        {"H:/nested.inc","{$I values.inc}"},
        {"H:/runtime.inc","n:=7 div 0;"},
        {"H:/open.inc","const split:array[1..2]of integer=(11,"},
        {"H:/after.inc","const after=1;"},
        {"H:/tail.inc","const extra=1;"}
    };
    uint8_t i,file,found=0,digest[32];uint16_t at,n,position;unsigned reads;Sha256 hash;

    disk_sources=files;disk_source_count=sizeof(files)/sizeof(files[0]);disk_reads=0;
    files[2].text="const across:array[1..2]of integer=(11,";
    files[1].text="const later=1;";
    load("{$I values.inc}\n22);\n{$I more.inc}\nbegin writeln(across[1],across[2])end.");
    check(compile(&s,&b,&d));build_close(&b);
    files[2].text="11,22";
    files[1].text="procedure more;begin part;n:=n+table[2]end;";
    /* Match the nine source/compiler pages left by the native kernel,
     * application and sixteen code banks. All three includes must fit. */
    {
        uint8_t held[23],j;
        load("const data:array[1..2]of integer=(\n{$I values.inc}\n);\n"
             "{$I open.inc}\n22);\n{$I after.inc}\n"
             "begin writeln(data[1],split[2],after)end.");
        for(j=0;j<23;++j){held[j]=page_new(PAGE_RUN);check(held[j]!=255);}
        check(compile(&s,&b,&d)&&b.sources.count==4&&build_ready(&s,&b));build_close(&b);
        for(j=0;j<23;++j)check(page_drop(held[j]));
        check(pages_used()==1);
    }
    /* A near-full root needs a separate reference page when its packed
     * catalog grows. Preserve its text and source identity through spilling. */
    {
        char text[16384];uint16_t length=0,j,k;unsigned required;uint8_t hash[32],after[32];
        for(j=0;j<159;++j){text[length++]='{';for(k=0;k<98;++k)text[length++]='x';text[length++]='}';text[length++]='\n';}
        strcpy(text+length,"var n:integer;\n{$I tail.inc}\nbegin n:=extra;writeln(n)end.");
        load(text);test_source_hash(&s,hash);allocation_calls=0;
        check(compile(&s,&b,&d)&&b.sources.count==2&&!b.sources.base);
        required=allocation_calls;check(build_check(&s,&b));build_close(&b);
        for(j=0;j<required;++j) {
            fail_after=j;check(!compile(&s,&b,&d)&&d.code&&!b.valid);
            fail_after=-1;check(pages_used()==1);
            test_source_hash(&s,after);check(!memcmp(hash,after,32));
        }
        /* This root fits33 references until its include grows the catalog.
         * Cancel after32 records have copied into the newly owned page. */
        length=0;
        for(j=0;j<154;++j){text[length++]='{';for(k=0;k<98;++k)text[length++]='x';text[length++]='}';text[length++]='\n';}
        length+=(uint16_t)snprintf(text+length,sizeof(text)-length,"var ");
        for(j=0;j<33;++j)length+=(uint16_t)snprintf(text+length,sizeof(text)-length,"%sv%03u%s",j?",":"",(unsigned)j,j%8==7?"\n":"");
        strcpy(text+length,":integer;\n{$I tail.inc}\nbegin v000:=extra;writeln(v000)end.");
        load(text);test_source_hash(&s,hash);spill_polls=0;cancel_stage=6;cancel_result=1;
        check(!compile_project(&s,&b,&d,1,0)&&d.code==COMPILE_INTERRUPTED);
        cancel_stage=0;check(spill_polls==2&&pages_used()==1);
        test_source_hash(&s,after);check(!memcmp(hash,after,32));
        check(pages_used()==1);
    }
    source_catalog_cases();
    replaced_source_cases();
    packed_catalog_cases();
    load("program Main;\n{$IPART.INC}\n{$I more.inc}\nbegin more;writeln(greeting,number,table[2])end.");
    check(compile(&s,&b,&d));check(b.sources.count==3);check(map_word(8)==1);check(map_word(14)==3);
    check(sources_find(&b.sources,"H:/PART.INC")==1&&sources_find(&b.sources,"H:/more.inc")==2);
    at=96;
    for(i=0;i<3;++i){check(map_word(at)==i);n=map_word(at+2);check(n&&n<=127);at+=8+n;}
    check(at==96+map_word(84));
    for(at=96+map_word(84);at<b.map_length;at+=16) {
        n=map_word(at+4);check(n<3);found|=(uint8_t)(1u<<n);
        check(build_location(&s,&b,map_word(at),&file,&position)&&file==n&&position==map_word(at+8));
        if(n)check(!build_locate(&s,&b,map_word(at),&position));
    }
    check(found==7&&build_ready(&s,&b));reads=disk_reads;
    files[1].text="procedure more;begin n:=99 end;";check(build_ready(&s,&b)&&disk_reads==reads);
    files[1].text="procedure more;begin part;n:=n+table[2]end;";check(build_check(&s,&b));
    disk_source_count=0;check(build_ready(&s,&b));disk_source_count=sizeof(files)/sizeof(files[0]);
    build_close(&b);check(pages_used()==1);
    load("{$R-,X+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d));
    check(cc.absolute_mode&&cc.io_check&&!cc.range_check&&cc.var_strict&&cc.with_limit==4&&cc.index_speed&&!cc.input_fresh&&!cc.user_interrupt);
    build_close(&b);
    load("{$U+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d)&&cc.user_interrupt&&cc.interrupt_entry);build_close(&b);
    /* Replay the same initializer offsets from separate include sites, each
     * with distinct inherited switches and lexical scopes. */
    files[5].text="const local:array[1..2]of integer=(11,22);";
    load("procedure first;{$R-,X+}\n{$I scope}\nbegin writeln(local[1])end;\n"
         "procedure second;{$R+,X-}\n{$I scope}\nbegin writeln(local[2])end;\n"
         "begin first;second end.");
    check(compile(&s,&b,&d)&&b.sources.count==2&&build_ready(&s,&b));
    printf("PERF02 repeated-include image-id=");
    printf("bytes=%u\n",b.length);build_close(&b);
    files[5].text="{$U-}const local=1;";
    load("{$U+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d)&&cc.user_interrupt&&cc.interrupt_entry);build_close(&b);
    load("{$U+}begin {$U-}end.");check(compile(&s,&b,&d)&&!cc.user_interrupt&&cc.interrupt_entry);build_close(&b);
    files[5].text="{$C-}";
    load("{$C+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d));
    check(cc.console_control&&image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    files[5].text="{$C-}var local:integer;";
    load("{$C+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d));
    check(cc.console_control&&image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    files[5].text="{$C+}";
    load("{$C-}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d));
    check(!cc.console_control&&image_get(&b,RT_CBREAK-0x4000u)==0);build_close(&b);
    load("{$C+}\n{$I scope}\n{$C-}begin end.");check(compile(&s,&b,&d));
    check(!cc.console_control&&image_get(&b,RT_CBREAK-0x4000u)==0);build_close(&b);
    load("const data:array[1..2]of integer=(\n{$I values.inc}\n);\nbegin writeln(data[1])end.");
    check(compile(&s,&b,&d)&&b.sources.count==2);build_close(&b);
    load("var n:integer;begin read(\n{$I arg.inc}\n,n)end.");
    check(compile(&s,&b,&d)&&b.sources.count==2&&cc.temporaries==0);build_close(&b);
    load("program P;\n{$I bad.inc}\nbegin end.");check(!compile(&s,&b,&d)&&d.code==56);
    check(!strcmp(compile_error_path(),"H:/bad.inc")&&d.start==13&&pages_used()==1);
    sha_init(&hash);for(at=0;files[4].text[at];++at)sha_byte(&hash,(uint8_t)files[4].text[at]);sha_end(&hash,digest);
    check(!strcmp(compile_error_path(),"H:/bad.inc"));
    load("{$I missing}\nbegin end.");check(!compile(&s,&b,&d)&&d.code==90);
    check(!strcmp(compile_error_path(),"H:/missing.PAS")&&pages_used()==1);
    invalid("{$I nested.inc}\nbegin end.",93);check(!strcmp(compile_error_path(),"H:/nested.inc"));
    invalid("{$I SLICE.PAS}\nbegin end.",93);
    invalid("begin {$I values.inc}end.",93);
    disk_sources=0;disk_source_count=0;
}
static uint16_t inline_word(uint16_t at) { return image_get(&b,at)|((uint16_t)image_get(&b,at+1)<<8); }
static uint16_t inline_marker(void) {
    uint16_t at,found=0;
    for(at=0x4000;at+4<b.length;++at)
        if(image_get(&b,at)==0xde&&image_get(&b,at+1)==0xad&&image_get(&b,at+2)==0xbe&&image_get(&b,at+3)==0xef) {
            check(!found);found=at+4;
        }
    check(found!=0);return found;
}
static void inline_corpus(void) {
    const char *names[]={"inlbytes","inladdr","inlframe","inlstd","inlerror"};
    char text[16384],path[128];unsigned i;FILE *file;size_t length;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d));check(cc.temporaries==0);build_close(&b);
    }
    {
        HostSource part={"H:/source-parts/inlnops.inc",text};
        char root[1024];
        file=fopen("tests/native/source-parts/inlnops.inc","rb");check(file!=NULL);
        length=fread(text,1,sizeof(text)-1,file);text[length]=0;check(feof(file)&&!ferror(file));check(fclose(file)==0);
        file=fopen("tests/native/inlpages.pas","rb");check(file!=NULL);
        length=fread(root,1,sizeof(root)-1,file);root[length]=0;check(feof(file)&&!ferror(file));check(fclose(file)==0);
        disk_sources=&part;disk_source_count=1;
        load(root);check(compile(&s,&b,&d));check(b.code_bytes>0x4000&&b.code_origin>GLOBAL_BASE);
        check(build_check(&s,&b)&&cc.stage_page==255&&b.sources.count==2);build_close(&b);
        disk_sources=NULL;disk_source_count=0;
    }
}
static void inline_cases(void) {
    static const uint8_t constants[]={0,255,0,1,255,255,255,255,0,1,0x34,0x44,0,0,0,0,0x80,0,0x80};
    const char *bad[]={"", "'A'", "1.25", "true", "Integer", "WriteLn", "--1", "(1)", "1*2", "missing", "1/", "65535", "+32768", "-32769", "$10000", "<$10000", "1 div 2", "^A", "+'A'", "nil"};
    const uint8_t errors[]={74,22,22,22,74,74,25,74,5,74,74,56,56,56,56,56,5,22,25,74};
    uint16_t at,i,j,typed=0,routine=0,delta,value;int coefficient;char text[1024],expression[160];
    load("var pad:array[1..301]of byte;begin\ninline($DE/$AD/$BE/$EF/0/255/256/-1/$FFFF/1+255/<$1234/>$44/1-1/$FFFF+1/32767+1/-32768/$CA/$FE/$BA/$BE)end.");
    check(compile(&s,&b,&d));at=inline_marker();
    for(i=0;i<sizeof(constants);++i)check(image_get(&b,at+i)==constants[i]);
    check(image_get(&b,at+i)==0xca&&image_get(&b,at+i+3)==0xbe&&build_check(&s,&b));build_close(&b);
    load("var g:array[1..301]of byte;const t:integer=17;\nprocedure p;begin end;procedure ext;external $2345;\nbegin inline($DE/$AD/$BE/$EF/g/g-g/t/t-t/p/p-p/ext/0-*/*+*/0-*-*/*+*-*/<*/<0-*/>1/$CA/$FE/$BA/$BE)end.");
    check(compile(&s,&b,&d));at=inline_marker();delta=b.code_origin-CC_CODE_BASE;
    for(i=0;i<cc.symbol_count;++i) {
        if(cc.symbols[i].role==S_TYPED)typed=cc.symbols[i].address+delta;
        if(cc.symbols[i].role==S_PROCEDURE&&!routine)routine=cc.symbols[i].address+delta;
    }
    check(typed&&routine);check(inline_word(at)==GLOBAL_BASE&&inline_word(at+2)==0);at+=4;
    check(inline_word(at)==typed&&inline_word(at+2)==0);at+=4;
    check(inline_word(at)==routine&&inline_word(at+2)==0&&inline_word(at+4)==0x2345);at+=6;
    check(inline_word(at)==(uint16_t)(0u-(at+0x4000u)));at+=2;
    check(inline_word(at)==(uint16_t)(2u*(at+0x4000u)));at+=2;
    check(inline_word(at)==(uint16_t)(0u-2u*(at+0x4000u)));at+=2;
    check(inline_word(at)==at+0x4000u);at+=2;
    check(image_get(&b,at)==(uint8_t)(at+0x4000u));++at;
    check(image_get(&b,at)==(uint8_t)(0u-(at+0x4000u)));++at;
    check(inline_word(at)==1&&image_get(&b,at+2)==0xca);check(build_check(&s,&b));build_close(&b);
    load("begin inline($DE/$AD/$BE/$EF/1+-1/1--1/+$FFFF/-$FFFF/-$8000/$CA/$FE/$BA/$BE)end.");
    check(compile(&s,&b,&d));at=inline_marker();
    check(image_get(&b,at)==0&&image_get(&b,at+1)==2&&inline_word(at+2)==65535);
    check(image_get(&b,at+4)==1&&inline_word(at+5)==0x8000&&image_get(&b,at+7)==0xca);build_close(&b);
    for(coefficient=-20;coefficient<=20;++coefficient)for(j=0;j<2;++j) {
        strcpy(expression,"0");
        for(i=0;i<(uint16_t)(coefficient<0?-coefficient:coefficient);++i)strcat(expression,coefficient<0?"-*":"+*");
        snprintf(text,sizeof(text),"var pad:array[1..511]of byte;begin inline($DE/$AD/$BE/$EF/%c%s/$CA/$FE/$BA/$BE)end.",j?'<':'>',expression);
        load(text);check(compile(&s,&b,&d));at=inline_marker();value=(uint16_t)((uint32_t)(uint16_t)coefficient*(at+0x4000u));
        check(j?image_get(&b,at)==(uint8_t)value:inline_word(at)==value);
        check(image_get(&b,at+(j?1:2))==0xca&&build_check(&s,&b));build_close(&b);
    }
    for(i=0;i<sizeof(errors);++i) {
        snprintf(text,sizeof(text),"begin inline(%s)end.",bad[i]);invalid(text,errors[i]);
    }
    invalid("var n:integer;begin inline(-n)end.",25);
    invalid("type t=(one,two);begin inline(one)end.",22);
    invalid("var r:record n:integer end;begin with r do inline(n)end.",74);
    invalid("procedure p;forward;procedure p;external $1234;begin end.",12);
    invalid("begin inline end.",4);
    {
        char large[10000];uint16_t n=0;
        strcpy(large,"begin inline(\n");n=(uint16_t)strlen(large);
        for(i=0;i<4200;++i) {
            large[n++]='*';if(i!=4199)large[n++]='/';
            if(i%32==31)large[n++]='\n';
        }
        strcpy(large+n,")end.");invalid(large,99);
    }
    check(pages_used()==1);
}
#include "compiler-job-cases.h"
/* Negative integer immediates must match their hexadecimal bit patterns.
 * Depth fixtures pin the pre-fold parser's resource/error-location boundary. */
static void literal_folding_cases(void) {
    static const char *expressions[]={"-0","-1","-2","-32767","-32768","-$0000","-$0001","-$7FFF","-$8000","-$FFFF"};
    static const uint16_t words[]={0,65535u,65534u,32769u,32768u,0,65535u,32769u,32768u,1};
    static const struct {const char *expression;uint8_t depth,ok,error;uint16_t first,last;} boundaries[]={
        {"1",19,1,0,0,0},
        {"1",20,1,0,0,0},
        {"1",21,1,0,0,0},
        {"1",22,0,99,36,37},
        {"-1",19,1,0,0,0},
        {"-1",20,1,0,0,0},
        {"-1",21,0,99,36,37},
        {"-1",22,0,99,36,37},
        {"-$8000",19,1,0,0,0},
        {"-$8000",20,1,0,0,0},
        {"-$8000",21,0,99,36,41},
        {"-$8000",22,0,99,36,37},
        {"-32768",19,1,0,0,0},
        {"-32768",20,1,0,0,0},
        {"-32768",21,1,0,0,0},
        {"-32768",22,0,99,36,37},
        {"+32768",19,0,56,34,39},
        {"+32768",20,0,56,35,40},
        {"+32768",21,0,99,36,41},
        {"+32768",22,0,99,36,37},
        {"-32769",19,0,56,34,39},
        {"-32769",20,0,56,35,40},
        {"-32769",21,0,99,36,41},
        {"-32769",22,0,99,36,37},
        {"-(32768)",19,0,56,35,40},
        {"-(32768)",20,0,99,36,41},
        {"-(32768)",21,0,99,36,37},
        {"-(32768)",22,0,99,36,37},
    };
    uint8_t expected[PAGE_SIZE];char text[256];unsigned i,j;
    uint16_t length,origin;uint8_t ok;
    for(i=0;i<sizeof(words)/sizeof(words[0]);++i) {
        snprintf(text,sizeof(text),"var n:integer;begin n:=$%04X end.",(unsigned)words[i]);
        load(text);check(compile(&s,&b,&d));length=b.code_bytes;origin=b.code_origin-0x4000u;
        check(length<=sizeof(expected));
        for(j=0;j<length;++j)expected[j]=image_get(&b,origin+j);
        build_close(&b);
        snprintf(text,sizeof(text),"var n:integer;begin n:=%s end.",expressions[i]);
        load(text);check(compile(&s,&b,&d));check(b.code_bytes==length);
        origin=b.code_origin-0x4000u;
        for(j=0;j<length;++j)check(image_get(&b,origin+j)==expected[j]);
        build_close(&b);check(pages_used()==1);
    }
    for(i=0;i<sizeof(boundaries)/sizeof(boundaries[0]);++i) {
        char *at=text;at+=snprintf(at,sizeof(text),"begin writeln(");
        for(j=0;j<boundaries[i].depth;++j)*at++='(';
        at+=snprintf(at,sizeof(text)-(size_t)(at-text),"%s",boundaries[i].expression);
        for(j=0;j<boundaries[i].depth;++j)*at++=')';strcpy(at,") end.");
        load(text);ok=compile(&s,&b,&d);
        check(ok==boundaries[i].ok&&d.code==boundaries[i].error&&
              d.start==boundaries[i].first&&d.end==boundaries[i].last);
        if(ok)build_close(&b);check(pages_used()==1);
    }
}

/* Compare negative immediates in consumers and every supported build kind. */
static void literal_context_cases(void) {
    static const char *pairs[][2]={
        {"var a:array[-2..2]of integer;begin a[-1]:=7 end.","var a:array[-2..2]of integer;begin a[$FFFF]:=7 end."},
        {"var i:integer;begin for i:=-2 to -1 do writeln(i)end.","var i:integer;begin for i:=$FFFE to $FFFF do writeln(i)end."},
        {"var n:integer;begin if n< -1 then n:=-2 end.","var n:integer;begin if n<$FFFF then n:=$FFFE end."},
        {"begin writeln(-2)end.","begin writeln($FFFE)end."},
        {"var s:set of 0..7;begin s:=[-$FFFF..2]end.","var s:set of 0..7;begin s:=[1..2]end."},
        {"var a:array[-2..2]of integer;n:integer;begin n:=sizeof(a[-1])end.","var a:array[-2..2]of integer;n:integer;begin n:=sizeof(a[$FFFF])end."}
    };
    uint8_t expected[PAGE_SIZE];unsigned i,j,mode;
    uint16_t length,origin,globals,image_length,map_length;
    CompileOptions options={0x9000,0xfbffu,COMPILE_PROGRAM};
    for(mode=0;mode<3;++mode)for(i=0;i<sizeof(pairs)/sizeof(pairs[0]);++i) {
        options.kind=mode==2?COMPILE_CHAIN:COMPILE_PROGRAM;
        load(pairs[i][1]);check(compile_project(&s,&b,&d,0,mode?&options:0));
        check(build_check(&s,&b));length=b.code_bytes;globals=b.globals_bytes;image_length=b.length;map_length=b.map_length;
        origin=mode==2?96:b.code_origin-0x4000u;check(length<=sizeof(expected));
        for(j=0;j<length;++j)expected[j]=image_get(&b,origin+j);
        build_close(&b);
        load(pairs[i][0]);check(compile_project(&s,&b,&d,0,mode?&options:0));
        check(build_check(&s,&b));check(b.code_bytes==length&&b.globals_bytes==globals&&b.length==image_length);
        check(b.map_length==map_length);check(build_is_chain(&b)==(mode==2));
        origin=mode==2?96:b.code_origin-0x4000u;
        for(j=0;j<length;++j)check(image_get(&b,origin+j)==expected[j]);
        build_close(&b);check(pages_used()==1);
    }
}

int main(int argc,char **argv) {
    unsigned i;
    uint16_t position;
    uint8_t old;
    FILE *file;
    check(argc==2);
    check(source_new(&s,"H:/SLICE.PAS"));
    literal_folding_cases();literal_context_cases();
    source_edit_cases();range_move_cases();
    source_hash_cases();
    stream_hash_cases();
    lookup_cases();
    lexer_cases();
    reference_cases();
    capture_cases();
    compiler_job_cases();
    large_job_capacity();
    memory_overlays();
    keyboard_corpus();
    interruption_corpus();
    compile_cancellation();
    runtime_initialization();
    compilation_options();
    chain_cases();
    chain_call_cases();
    startup_cases();
    console_boundary();console_elision();
    bridge_corpus();
    hook_corpus();
    index_modes();
    source_resume_cases();
    include_cases();
    inline_cases();
    inline_corpus();
    load("var a:array(.1..2.)of integer;s:string(.3.);t:set of byte;\n"
         "begin a(.1.):=7;s:='abc';t:=(.1..2.);\n"
         "if(a(.1.)=7)and(1 in t)then writeln(s(.1.))end.");
    check(compile(&s,&b,&d));build_close(&b);check(pages_used()==1);
    invalid("begin writeln(1.)end.",5);
    check(d.start==15);
    invalid("begin writeln(1.0.)end.",57);
    check(d.start==14);
    load("begin writeln(1. );writeln(1.E1 )end.");
    check(compile(&s,&b,&d));build_close(&b);check(pages_used()==1);
    load("begin  \r\nend. \r\n\032padding"); check(s.length==11);
    check(source_replace(&s,0,0,NULL,0)&&!s.dirty);
    {
        const uint8_t embedded[]="begin write('a\0b') end.";
        check(source_import(&s,embedded,sizeof(embedded)-1,&d));
        check(compile(&s,&b,&d)); build_close(&b);
    }
    invalid("var r:real;begin r:=true end.",44);
    invalid("begin Input:=Output end.",54);
    invalid("begin read(Output,true)end.",41);
    invalid("var i:integer;begin rename(i,'x')end.",21);
    invalid("var f:text;begin rename(f)end.",3);
    invalid("var f,g:text;\nbegin f:=g end.",54);
    invalid("var f:text;\nbegin f:=1 end.",54);
    invalid("var f:text;\nbegin writeln(1+f)end.",54);
    invalid("type f=file;\nprocedure p(a:f);\nbegin end;\nbegin end.",67);
    invalid("function f:text;\nbegin end;\nbegin end.",67);
    invalid("type a=file;\nf=file of a;\nbegin end.",68);
    invalid("const f:text='X';\nbegin end.",61);
    invalid("type r=record f:text end;\nconst a:r=(f:'X');\nbegin end.",61);
    invalid("type a=file of integer;\nb=file of real;\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    invalid("type a=file of integer;\nb=file of byte;\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    invalid("type a=file of integer;\nb=file;\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    invalid("type b=file;\nvar f:text;\nprocedure p(var g:b);\nbegin end;\nbegin p(f)end.",44);
    invalid("type a=file of integer;\nc=array[1..2]of byte;\nb=file of c;\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    invalid("type a=file of string[10];\nb=file of string[10];\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    load("type plain=file;\nints=file of integer;\nother=file of integer;\nz=record end;\nzero=file of z;\nvar a:text;\nb:plain;\nc:ints;\nd:other;\ne:zero;\nprocedure p(var f:ints);\nbegin end;\nbegin p(d);\nwriteln(sizeof(text),sizeof(a),sizeof(b),sizeof(c),sizeof(e))end.");
    check(compile(&s,&b,&d)); check(b.globals_bytes==82);
    check(cc.text_type!=255&&cc.types[cc.text_type].family==K_FILE&&cc.types[cc.text_type].width==16);
    check(cc.types[cc.text_type].capacity==FILE_TEXT); build_close(&b);
    load("type r=record f:text end;\nar=array[1..2]of r;\nff=file of ar;\nvar a,b:r;\nc:ar;\nprocedure p(x:ar);\nbegin end;\nbegin a:=b;\np(c);\nwriteln(sizeof(ff))end.");
    check(compile(&s,&b,&d)); check(b.globals_bytes==96); build_close(&b);
    load("var Input,Output:integer;begin Input:=7;Output:=Input;if Output<>7 then halt end.");
    check(compile(&s,&b,&d)); check(b.globals_bytes==4); build_close(&b);
    load("var kbd:integer;begin kbd:=7;writeln(kbd)end.");
    check(compile(&s,&b,&d));check(b.globals_bytes==2&&!cc.file_entry);build_close(&b);
    load("var c:char;procedure p(var f:text);begin read(f,c)end;begin p(Kbd)end.");
    check(compile(&s,&b,&d));check(cc.file_entry&&cc.temporaries==0);build_close(&b);
    load("var c:char;procedure p(var f:text);begin read(f,c)end;begin p(Con);p(Trm);write(Con,c);write(Trm,c)end.");
    check(compile(&s,&b,&d));check(cc.file_entry&&cc.temporaries==0);build_close(&b);
    load("var Con,Trm:integer;begin Con:=1;Trm:=Con end.");
    check(compile(&s,&b,&d));check(b.globals_bytes==4&&!cc.file_entry);build_close(&b);
    load("var Lst,Aux:integer;begin Lst:=1;Aux:=Lst end.");
    check(compile(&s,&b,&d));check(b.globals_bytes==4&&!cc.file_entry);build_close(&b);
    load("procedure p(var f:text);begin writeln(f,'x')end;begin p(Lst);p(Aux)end.");
    check(compile(&s,&b,&d));check(cc.file_entry&&cc.temporaries==0);build_close(&b);
    load("type text=integer;\nvar n:text;\nbegin n:=7;\nwriteln(n)end.");
    check(compile(&s,&b,&d)); check(cc.text_type==255); build_close(&b);
    load("type ints=file of integer;raw=file;z=record end;zero=file of z;\nblock=array[1..300]of byte;wide=file of block;\nvar t:text;f:ints;u:raw;e:zero;w:wide;begin end.");
    check(compile(&s,&b,&d));
    for(i=0;i<5;++i) {
        unsigned j;
        const uint8_t family[]={FILE_TEXT,FILE_TYPED,FILE_UNTYPED,FILE_TYPED,FILE_TYPED};
        const uint16_t width[]={0,2,128,0,300};
        for(j=0;j<16;++j) {
            uint8_t expected=j==0?0x5a:j==1?0x46:j==8?family[i]:j==10?(uint8_t)width[i]:j==11?(uint8_t)(width[i]>>8):0;
            check(image_get(&b,0x4000u+i*16+j)==expected);
        }
    }
    check(cc.types[cc.text_type].high==0); build_close(&b);
    load("var p:^text;begin getmem(p,16)end.");
    check(compile(&s,&b,&d)); check(cc.types[cc.text_type].high==0); build_close(&b);
    load("var p:^text;begin new(p)end.");
    check(compile(&s,&b,&d)); check(cc.types[cc.text_type].high!=0); build_close(&b);
    invalid("var n:integer;begin n:=1/2 end.",44);
    invalid("var r:real;begin r:=1e38 end.",57);
    invalid("var r:real;begin r:=1e-39 end.",57);
    invalid("var r:real;begin r:=1e end.",57);
    invalid("var r:real;begin r:=1e001 end.",1);
    invalid("var r:real;begin r:=1.0e38 end.",57);
    invalid("type t=1.0..2.0;begin end.",57);
    invalid("begin writeln(real(1))end.",30);
    invalid("begin writeln(integer(1.5))end.",31);
    invalid("begin writeln(ord(1.0))end.",31);
    invalid("begin writeln(sqrt(true))end.",23);
    invalid("begin writeln(sqrt('x'))end.",23);
    invalid("begin writeln(sqrt('xx'))end.",23);
    invalid("begin writeln(ln(true))end.",23);
    invalid("begin writeln(exp('x'))end.",23);
    invalid("begin writeln(sin(true))end.",23);
    invalid("begin writeln(cos('x'))end.",23);
    invalid("begin writeln(arctan('xx'))end.",23);
    invalid("begin writeln(random())end.",41);
    invalid("begin writeln(random(1.5))end.",23);
    invalid("begin writeln(random(true))end.",23);
    invalid("begin writeln(random('A'))end.",23);
    invalid("type t=(a,b);begin writeln(random(a))end.",23);
    invalid("begin writeln(random(1,2))end.",5);
    invalid("begin randomize()end.",1);
    invalid("begin writeln(paramcount())end.",5);
    invalid("begin writeln(paramstr)end.",4);
    invalid("begin writeln(paramstr())end.",54);
    invalid("begin writeln(paramstr(1,2))end.",5);
    invalid("begin writeln(paramstr(1.5))end.",23);
    invalid("begin writeln(paramstr(true))end.",23);
    invalid("begin writeln(paramstr('A'))end.",23);
    invalid("begin writeln(paramstr('text'))end.",23);
    invalid("type t=(a,b);begin writeln(paramstr(a))end.",23);
    load("var a:array[0..1]of byte;begin writeln(sizeof(a[paramcount+length(paramstr(1))]))end.");
    check(compile(&s,&b,&d));check(!cc.parameter_entry);build_close(&b);
    load("var paramcount:integer;\nfunction paramstr(n:integer):integer;begin paramstr:=n end;\nbegin paramcount:=7;writeln(paramstr(paramcount))end.");
    check(compile(&s,&b,&d));check(!cc.parameter_entry);build_close(&b);
    load("begin writeln(paramcount,paramstr(1))end.");
    check(compile(&s,&b,&d));check(cc.parameter_entry&&cc.temporaries==0);ordered_map();build_close(&b);
    load("begin end.");check(compile(&s,&b,&d));check(!cc.parameter_entry);build_close(&b);
    load("var f:file of integer;a:array[0..1]of integer;begin writeln(sizeof(a[filepos(f)]))end.");
    check(compile(&s,&b,&d));check(!cc.file_entry&&cc.temporaries==0);build_close(&b);
    load("var f:integer;function filepos(n:integer):integer;begin filepos:=n end;\nbegin f:=7;writeln(filepos(f))end.");
    check(compile(&s,&b,&d));check(!cc.file_entry);build_close(&b);
    invalid("var f:file of integer;begin assign(f,1)end.",33);
    invalid("var f:integer;begin reset(f)end.",21);
    invalid("begin writeln(eof())end.",21);
    invalid("begin writeln(eoln(1))end.",21);
    invalid("var f:file of integer;begin writeln(seekeof(f))end.",66);
    invalid("begin writeln(filepos)end.",4);
    invalid("begin writeln(keypressed())end.",5);
    load("var a:array[0..1]of integer;begin writeln(sizeof(a[ord(keypressed)]))end.");
    check(compile(&s,&b,&d));check(cc.keyboard_entry&&!cc.file_entry&&cc.hook_character);build_close(&b);
    load("function keypressed:boolean;begin keypressed:=true end;begin writeln(keypressed)end.");
    check(compile(&s,&b,&d));check(cc.keyboard_entry&&!cc.file_entry&&cc.hook_character);build_close(&b);
    load("begin writeln(keypressed)end.");
    check(compile(&s,&b,&d));check(cc.keyboard_entry&&cc.keyboard_poll&&!cc.file_entry&&cc.temporaries==0);
    {
        uint16_t at=cc_runtime_private(RP_INPUT_KEY)-0x4000u+1;
        check((far_get(b.pages[0],at)|((uint16_t)far_get(b.pages[0],at+1)<<8))==
              cc.keyboard_entry+b.code_origin-CC_CODE_BASE);
    }
    build_close(&b);
    load("var a:array[0..1]of integer;begin writeln(sizeof(a[ord(eof)]))end.");
    check(compile(&s,&b,&d));check(!cc.file_entry&&cc.temporaries==0);build_close(&b);
    invalid("var f:file of integer;begin seek(f,true)end.",23);
    invalid("var f:file of integer;begin write(f,1)end.",41);
    invalid("var f:file of integer;b:byte;begin write(f,b)end.",44);
    invalid("var f:file;b:byte;begin blockread(f,b,1,b)end.",24);
    load("var a:array[0..1]of integer;begin writeln(sizeof(a[random(2)]))end.");
    check(compile(&s,&b,&d)); check(!cc.random_entry&&!cc.random_state);
    build_close(&b);
    invalid("begin writeln(succ(1.0))end.",31);
    invalid("begin writeln(1.0 div 2)end.",47);
    invalid("begin writeln(1.0 and 2)end.",47);
    invalid("var r:real;begin for r:=1 to 2 do end.",30);
    invalid("type a=array[real]of byte;begin end.",30);
    invalid("type a=set of real;begin end.",30);
    invalid("begin writeln(1:5:2)end.",5);
    invalid("begin writeln('x':5:2)end.",5);
    invalid("begin writeln(true:5:2)end.",5);
    invalid("begin writeln('xx':5:2)end.",5);
    invalid("begin writeln(1:true)end.",23);
    invalid("begin writeln(1.0:5:true)end.",23);
    invalid("var s:string[80];begin str(true,s)end.",26);
    invalid("var s:string[80];begin str('x',s)end.",26);
    invalid("var s:string[80];begin str('xx',s)end.",26);
    invalid("var s:string[80];begin str(1:5:2,s)end.",3);
    invalid("var c:char;begin str(1,c)end.",34);
    invalid("begin str(1,'x')end.",41);
    invalid("type t=(a,b);begin writeln(1:a)end.",23);
    invalid("var b:byte;c:integer;begin val('1',b,c)end.",27);
    invalid("type s=1..5;var n:s;c:integer;begin val('1',n,c)end.",27);
    invalid("var b:char;c:integer;begin val('1',b,c)end.",27);
    invalid("var n:integer;c:byte;begin val('1',n,c)end.",24);
    invalid("type s=0..5;var n:integer;c:s;begin val('1',n,c)end.",24);
    invalid("var n,c:real;begin val('1',n,c)end.",24);
    invalid("var c:integer;begin val('1',1,c)end.",41);
    invalid("var n:integer;begin val('1',n,0)end.",41);
    invalid("var n,c:integer;begin val(1,n,c)end.",33);
    invalid("begin goto missing end.",40);
    invalid("label l;begin goto l end.",40);
    invalid("label l,l;begin end.",43);
    invalid("label l;begin l:;l:end.",43);
    invalid("label l;var l:integer;begin end.",43);
    invalid("begin missing:end.",41);
    invalid("label l;var r:record l:integer end;begin with r do begin l:l:=1 end end.",7);
    invalid("label l;procedure p;begin goto l end;begin l:end.",72);
    invalid("label l;procedure p;begin l:end;begin l:end.",72);
    invalid("label l;var i:integer;begin goto l;for i:=1 to 1 do l:halt end.",71);
    invalid("label l;var i:integer;begin if false then for i:=1 to 1 do l:halt;goto l end.",71);
    invalid("label 000;begin goto 0;000:end.",40);
    invalid("label $1;begin end.",58);
    invalid("label 1;begin goto $1;1:end.",40);
    invalid("label 1;begin $1:end.",41);
    invalid("begin writeln(12345678901234567890)end.",56);
    invalid("const x=-12345678901234567890;begin end.",56);
    load("label unused,01,1;begin goto 01;01:goto 1;1:end.");
    check(compile(&s,&b,&d)); ordered_map(); build_close(&b);
    invalid("var a,b:byte absolute $8000;begin end.",75);
    invalid("var b:byte absolute 'A';begin end.",22);
    invalid("var b:byte absolute true;begin end.",22);
    invalid("var b:byte absolute missing;begin end.",41);
    invalid("var b:byte absolute b;begin end.",41);
    invalid("var a:array[1..2]of byte;b:byte absolute a[2];begin end.",1);
    invalid("var n:integer;b:byte absolute n+1;begin end.",1);
    invalid("var n:integer;b:byte absolute addr(n);begin end.",41);
    invalid("function f:byte;var b:byte absolute f;begin end;begin end.",41);
    invalid("var n:integer;begin mem['A']:=0 end.",23);
    invalid("var n:integer;begin mem[addr(n)]:='A' end.",44);
    invalid("var n:integer;begin mem[addr(n)]:=true end.",44);
    invalid("begin port['A']:=0 end.",23);
    invalid("begin port[0]:='A' end.",23);
    invalid("begin writeln(addr(port[0]))end.",41);
    invalid("begin writeln(sizeof(port[0]))end.",41);
    invalid("begin writeln(sizeof(mem))end.",8);
    invalid("procedure p(var b:byte);begin end;begin p(port[0])end.",41);
    invalid("var a:byte;begin fillchar(a,1,ptr(0))end.",31);
    invalid("var a:byte;begin fillchar(a,'A',0)end.",23);
    invalid("var a,b:byte;begin move(a,b,true)end.",23);
    invalid("var a:byte;begin move('AB',a,1)end.",41);
    invalid("function f:byte;begin fillchar(f,1,0)end;begin end.",41);
    invalid("var p:integer;begin new(p)end.",44);
    invalid("begin new(nil)end.",44);
    invalid("begin release(0)end.",44);
    invalid("type p=^integer;var x:p;begin getmem(x,true)end.",23);
    invalid("begin stackptr:=true end.",23);
    invalid("begin writeln(addr(stackptr))end.",44);
    invalid("begin writeln(memavail())end.",5);
    invalid("type n=record p:^n end;begin end.",42);
    invalid("type n=record p:^later end;later=integer;begin end.",42);
    load("var p:^integer;integer:char;begin integer:='A';writeln(sizeof(p^))end.");
    check(compile(&s,&b,&d)); build_close(&b);
    load("type n=integer;r=record p:^n end;var x:r;begin x.p:=nil end.");
    check(compile(&s,&b,&d)); build_close(&b);
    invalid("type p=^missing;begin end.",42);
    invalid("type p=^later;var later:integer;begin end.",42);
    invalid("type p=^integer;const x:p=nil;begin end.",61);
    invalid("const x=nil;begin end.",1);
    invalid("type p=^integer;var x:p;begin x:=p(0)end.",30);
    invalid("type p=^integer;var x:p;begin x:=0 end.",44);
    invalid("type p=^integer;q=^char;var x:p;y:q;begin x:=y end.",44);
    invalid("type p=^integer;var x:p;begin writeln(x<nil)end.",47);
    invalid("type p=^integer;var x:p;begin x:=x+1 end.",47);
    invalid("type p=^integer;function f:p;begin f^:=1 end;begin end.",7);
    invalid("type p=^integer;function f:p;begin f:=nil end;begin writeln(f^)end.",66);
    invalid("type p=^integer;q=^char;var x:q;procedure f(var a:p);begin end;begin f(x)end.",44);
    invalid("begin writeln(succ(nil))end.",31);
    invalid("begin writeln(pred(nil))end.",31);
    invalid("begin writeln(succ(ptr(0)))end.",31);
    invalid("begin writeln(pred(ptr(0)))end.",31);
    invalid("begin writeln(succ([]))end.",31);
    invalid("begin writeln(pred([1]))end.",31);
    invalid("begin writeln(32768) end.",56);
    invalid("var s:string[3]; begin s[true]:='x' end.",23);
    invalid("var s:string[3]; begin s[1][1]:='x' end.",7);
    invalid("var n:integer; begin n[1]:=2 end.",7);
    invalid("var s:string[3]; begin for s[1]:='a' to 'z' do end.",30);
    invalid("type t=string[3]; function f:t; begin f[1]:='x' end; begin end.",7);
    invalid("type t=string[3]; function f:t; begin f:='x' end; begin writeln(f[1]) end.",5);
    invalid("type t=string[3]; procedure p(var s:t); begin end; function f:t; begin p(f) end; begin end.",41);
    invalid("begin writeln(concat()) end.",41);
    invalid("begin writeln(copy(3,1,2)) end.",33);
    invalid("begin writeln(copy('abc',true,2)) end.",23);
    invalid("var s:string[3]; begin delete(s[1],1,2) end.",34);
    invalid("var s:string[3]; begin insert('x',s[1],1) end.",34);
    invalid("var s:string[3]; begin s[1:='x' end.",9);
    invalid("var a:array[boolean] of byte; begin a[1]:=0 end.",44);
    invalid("var a:array[1..3] of byte; begin a[true]:=0 end.",44);
    invalid("type t=array[1..2] of byte; function f:t; begin end; begin end.",48);
    invalid("type a=array[1..2] of byte; b=array[1..2] of byte; var x:a; y:b; begin x:=y end.",44);
    invalid("var a,b:array[1..2] of byte; begin writeln(a=b) end.",62);
    invalid("var a:array[1..256] of char; begin writeln(length(a)) end.",62);
    invalid("var a:array[boolean] of char; begin writeln(length(a)) end.",62);
    invalid("var a:array['a'..'c'] of char; begin writeln(length(a)) end.",62);
    invalid("var a:array[1..3] of char; s:string[3]; begin a:=s end.",44);
    invalid("var a:array[1..3] of char; begin a:='ab' end.",60);
    invalid("var a:array[1..1] of char; begin a:='x' end.",60);
    invalid("var a:array[1..3] of char; begin a:='abc'+'d' end.",1);
    invalid("var a:array[1..3] of char; begin a:=('abc') end.",41);
    invalid("type t=array[integer] of byte; begin end.",98);
    invalid("var a,b:array[1..17000] of byte; begin a[1]:=1;b[1]:=2 end.",98);
    invalid("type t=array[string[3]] of byte; begin end.",30);
    invalid("const a:array[1..2] of integer=(1,2,3); begin end.",5);
    invalid("const a:array[1..3] of byte=(1,256,3); begin end.",44);
    invalid("type r=record a:integer end; var x:r; begin x.z:=1 end.",41);
    invalid("type r=record a,a:integer end; begin end.",43);
    invalid("type r=record case boolean of false:(n:integer);true:(n:char) end;begin end.",43);
    invalid("type a=record n:integer end; b=record n:integer end;var x:a;y:b;begin x:=y end.",44);
    invalid("type r=record n:integer end;function f:r;begin end;begin end.",48);
    invalid("type r=record n:integer end;var x,y:r;begin writeln(x=y) end.",62);
    invalid("type r=record a,b:integer end;const x:r=(b:2;a:1);begin end.",69);
    invalid("type r=record a,b,c:byte end;const x:r=(a:1;c:3);begin end.",69);
    invalid("type r=record case tag:boolean of false:(n:integer) end;const x:r=(n:1);begin end.",69);
    invalid("type r=record case missing of 0:(n:integer) end;begin end.",2);
    invalid("type r=record end;const x:r=();begin end.",41);
    invalid("var n:integer;begin with n do end.",29);
    invalid("{$W0}type r=record n:integer end;var x:r;begin with x do end.",97);
    invalid("{$W1}type r=record n:integer end;var x:r;begin with x,x do end.",97);
    invalid("type r=record n:integer end;var x:r;begin with x,x,x,x,x do end.",97);
    invalid("type r=record n:integer end;var x:r;begin {$W9}with x,x,x,x,x do end.",97);
    invalid("type r=record n:integer end;var x:r;begin with x do for n:=1 to 3 do end.",41);
    invalid("type r=record n:integer end;var x:r;begin for x.n:=1 to 3 do end.",30);
    invalid("type t=set of integer;begin end.",70);
    invalid("type t=set of -1..3;begin end.",70);
    invalid("type t=set of 0..256;begin end.",70);
    invalid("type t=set of byte;function f:t;begin end;begin end.",48);
    invalid("begin writeln(1 in [1,true])end.",44);
    invalid("begin writeln([1]=[true])end.",44);
    invalid("var a,b:set of byte;begin writeln(a<b)end.",47);
    invalid("type t=set of 3..5;const x:t=[0];begin end.",45);
    invalid("type t=set of byte;const x:t=[true];begin end.",44);
    invalid("const x=[1];begin end.",1);
    invalid("var s:set of byte;begin writeln(s)end.",66);
    invalid("type r=record n:integer end;var x:r;begin writeln(x)end.",62);
    invalid("type t=set of 3..5;u=set of 3..5;var x:t;procedure p(var v:u);begin end;begin p(x)end.",44);
    invalid("begin writeln(+32768) end.",56);
    invalid("begin writeln(-32769) end.",56);
    invalid("begin writeln(-(32768)) end.",56);
    invalid("begin writeln('unterminated) end.",55);
    invalid("begin {$Z+} end.",93);
    invalid("begin nope:=1 end.",41);
    invalid("var n,n: integer; begin end.",43);
    invalid("begin writeln(1) writeln(2) end.",1);
    invalid("program begin; begin end.",53);
    invalid("begin {unterminated",91);
    invalid("begin ! end.",58);
    invalid("var s:string[0]; begin end.",49);
    invalid("var s:string[256]; begin end.",49);
    invalid("var s:string; begin end.",8);
    invalid("var s:string[80; begin end.",9);
    invalid("var s:string[8]; begin s:=42 end.",44);
    invalid("var n:integer; begin n:='text' end.",44);
    invalid("var s:string[8]; begin writeln(s*2) end.",47);
    invalid("var s:string[8]; begin writeln(-s) end.",47);
    invalid("var n:boolean; begin readln(n) end.",66);
    invalid("type t=(a,b);var n:t;begin read(n)end.",66);
    invalid("var a:array[1..3]of char;begin readln(a)end.",66);
    invalid("begin readln(1)end.",41);
    invalid("begin readln()end.",41);
    invalid("begin read()end.",41);
    invalid("begin {$B?}end.",93);
    invalid("begin {$I?}end.",90);
    invalid("begin {$I}end.",90);
    invalid("begin {$I0}end.",90);
    invalid("begin {$J+}end.",93);
    load("var n:integer;begin {$I-}write(1);{$I+}write(2);read(n);n:=ioresult end.");
    check(compile(&s,&b,&d));ordered_map();
    check(calls(RT_IOCALL,0)==4&&calls(RT_IOCALL,1)==3&&calls(RT_IORESULT,-1)==1);
    check(cc.io_check==1&&cc.temporaries==0);build_close(&b);
    load("begin {$i-,b-,r+}writeln(ioresult)end.");
    check(compile(&s,&b,&d));check(!cc.io_check&&!cc.input_fresh&&cc.range_check);
    check(calls(RT_IOCALL,0)==6&&calls(RT_IOCALL,1)==0);build_close(&b);
    load("begin writeln(ioresult)end.");
    check(compile(&s,&b,&d));check(cc.io_check&&cc.input_fresh&&!cc.range_check);
    check(calls(RT_IOCALL,1)==3);build_close(&b);
    load("var ioresult:integer;begin ioresult:=7;writeln(ioresult)end.");
    check(compile(&s,&b,&d));check(!calls(RT_IORESULT,-1));build_close(&b);
    load("function ioresult:integer;begin ioresult:=7 end;begin writeln(ioresult)end.");
    check(compile(&s,&b,&d));check(!calls(RT_IORESULT,-1));build_close(&b);
    load("var a:array[0..1]of byte;begin writeln(sizeof(a[ioresult]))end.");
    check(compile(&s,&b,&d));check(!calls(RT_IORESULT,-1));build_close(&b);
    invalid("begin readln(missing) end.",41);
    invalid("begin if 1 then end.",20);
    invalid("begin while 'a' do end.",20);
    invalid("begin repeat until 1 end.",20);
    invalid("begin if true writeln(1) end.",17);
    invalid("begin while true writeln(1) end.",13);
    invalid("begin if true then writeln(1); else writeln(2) end.",1);
    invalid("var b:boolean; begin b:=1 end.",44);
    invalid("begin writeln(true+false) end.",47);
    invalid("begin writeln(1 and true) end.",47);
    invalid("begin writeln(not 'x') end.",47);
    invalid("begin writeln(1='x') end.",47);
    invalid("var i:integer; begin for i:=1 do end.",18);
    invalid("var i:integer; begin for i:=1 to 3 i:=2 end.",13);
    invalid("var i:integer; begin for i:=1 to 3 do i:=2 end.",44);
    invalid("var i:integer; begin for i:=1 to 3 do for i:=1 to 2 do end.",44);
    invalid("var i:integer; begin for i:=true to false do end.",44);
    invalid("const x=1+2; begin end.",1);
    invalid("const x=x; begin end.",41);
    invalid("const x=1; begin x:=2 end.",44);
    invalid("type t=integer; begin t:=2 end.",44);
    invalid("type t=5..1; begin end.",52);
    invalid("type t=1..true; begin end.",51);
    invalid("type t='ab'..'cd'; begin end.",51);
    invalid("type a=(red,green); b=(blue,yellow); var x:a; begin x:=blue end.",44);
    invalid("type a=(red,green); b=(blue,yellow); begin writeln(red=blue) end.",47);
    invalid("type a=(red,green); begin writeln(red) end.",44);
    invalid("const n:byte=256; begin end.",44);
    invalid("var n:integer; const x=n; begin end.",41);
    invalid("begin writeln(ord([])) end.",31);
    invalid("begin writeln(chr(true)) end.",23);
    invalid("begin writeln(#256) end.",56);
    invalid("begin writeln(#$100) end.",56);
    invalid("begin writeln($00000) end.",56);
    invalid("begin case 'text' of end end.",41);
    invalid("begin case 1 of end end.",41);
    invalid("begin case 1 of else end end.",41);
    invalid("begin case 1 end end.",15);
    invalid("begin case 1 of 1 end end.",3);
    invalid("begin case 1 of 'a': end end.",46);
    invalid("begin case 'a' of 1: end end.",46);
    invalid("begin case true of 1: end end.",46);
    invalid("begin case 1 of missing: end end.",41);
    invalid("var n:integer; begin case 1 of n: end end.",41);
    invalid("type t=(red,green); u=(blue,white); begin case red of blue: end end.",46);
    invalid("begin case 1 of 0..true: end end.",46);
    invalid("begin case 1 of 1:;",14);
    invalid("procedure p; forward; begin end.",73);
    invalid("procedure p; begin end; procedure p; begin end; begin end.",43);
    invalid("procedure p(a:integer); begin end; begin p end.",4);
    invalid("procedure p(a:integer); begin end; begin p(true) end.",44);
    invalid("procedure p(var a:integer); begin end; begin p(1) end.",44);
    invalid("var b:byte; procedure p(var a:integer); begin end; begin p(b) end.",44);
    invalid("procedure p(var a); begin a:=1 end; begin end.",44);
    invalid("procedure p(var a); begin writeln(a) end; begin end.",44);
    invalid("procedure p; var hidden:integer; begin end; begin hidden:=1 end.",41);
    invalid("begin writeln(length(1)) end.",44);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin writeln(f-1) end.",47);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin writeln(1+f) end.",47);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin writeln(f+true) end.",47);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin if f then end.",20);
    invalid("type text=string[3]; var c:char; function f:text; begin f:='a' end; begin for c:=f to 'z' do end.",44);
    invalid("type text=string[3]; var c:char; function f:text; begin f:='a' end; begin for c:='a' to f do end.",44);
    invalid("type text=string[3]; var s:string[4]; procedure p(var a:text); begin end; begin p(s) end.",44);
    load("type text=string[3]; function f:text; begin f:='abc' end; begin writeln(f) end.");
    check(compile(&s,&b,&d)); ordered_map(); build_close(&b);
    load("const x=-32768; y=x; z=-y; type octet=byte; text=string[3]; var n:octet; s:text; begin n:=257; s:='x'; writeln(x,y,z,n,s) end.");
    check(compile(&s,&b,&d)&&b.globals_bytes==5); build_close(&b);
    load("var b:boolean; n:integer; begin b:=not false; n:=1234; writeln(b,n) end.");
    check(compile(&s,&b,&d)&&b.globals_bytes==3); ordered_map(); build_close(&b);
    load("var i,n:integer; begin if true then if false then n:=1 else n:=2;\n"
         "for i:=1 to 3 do begin while false do n:=9; repeat n:=n-1 until n=0 end end.");
    check(compile(&s,&b,&d)); ordered_map(); build_close(&b);
    load("var a,b:string[80]; n:integer; c:string[255]; begin a:='Hello'; b:=a; c:=''; writeln(b,n,c); readln(a) end.");
    check(compile(&s,&b,&d)); check(b.globals_bytes==420&&b.code_origin==GLOBAL_BASE+420);
    build_close(&b);
    load("var writeln: integer; begin writeln:=1 end.");
    check(compile(&s,&b,&d)); build_close(&b);
    load("program Slice;\nvar n: integer;\nbegin n:=-32768; writeln('N=',n,',',7 div -3,',',7 mod -3) end.");
    {
    unsigned before_allocations=allocation_calls,required;
    check(compile(&s,&b,&d));required=allocation_calls-before_allocations;build_close(&b);
    check(required>0); /* Inject failure at every measured allocation. */
    for(i=0;i<required;++i) {
        fail_after=(int)i;
        check(!compile(&s,&b,&d)&&d.code==99); check(pages_used()==1);
    }
    fail_after=-1;
    }
    check(compile(&s,&b,&d)); check(build_check(&s,&b)); check(build_ready(&s,&b));
    check(b.length<=2u*PAGE_SIZE&&b.pages[2]==255&&b.versions[2]==0);
    old=b.pages[1];b.pages[1]=255;check(!build_check(&s,&b)&&!build_ready(&s,&b));b.pages[1]=old;
    b.pages[2]=old;check(!build_check(&s,&b)&&!build_ready(&s,&b));b.pages[2]=255;
    check(build_check(&s,&b)&&build_ready(&s,&b));
    old=far_get(b.map_page,b.map_length-1);
    far_set(b.map_page,b.map_length-1,old^1);
    check(!build_check(&s,&b));
    check(!build_ready(&s,&b));
    far_set(b.map_page,b.map_length-1,old); check(!build_check(&s,&b));b.versions[3]=page_versions[b.map_page];
    old=far_get(b.pages[1],0);
    far_set(b.pages[1],0,old^1); check(!build_check(&s,&b)); check(!build_ready(&s,&b));
    far_set(b.pages[1],0,old); check(!build_check(&s,&b));b.versions[1]=page_versions[b.pages[1]];
    check(!build_locate(&s,&b,b.code_bytes,&position));
    check(build_locate(&s,&b,0,&position));
    check(position==(uint16_t)(strstr("program Slice;\nvar n: integer;\nbegin n:=-32768;", "n:=-")-"program Slice;\nvar n: integer;\nbegin n:=-32768;"));
    file=fopen(argv[1],"wb"); check(file!=NULL);
    for(i=0;i<b.length;++i) fputc(image_get(&b,(uint16_t)i),file);
    check(!fclose(file));
    check(source_replace(&s,s.length,s.length,(const uint8_t*)" ",1));
    check(!build_check(&s,&b)); build_close(&b);
    load("begin\nwriteln(7 div 0)\nend.");
    check(source_replace(&s,5,5,(const uint8_t*)"   ",3));
    check(compile(&s,&b,&d));
    check(build_locate(&s,&b,0,&position)&&position==9);
    build_close(&b);
    {
        static uint8_t text[SOURCE_LIMIT];
        uint16_t n;
        file=fopen("tests/native/control.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        check(b.globals_bytes==29); ordered_map(); build_close(&b);
        file=fopen("tests/native/ordinals.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        check(b.globals_bytes==19); ordered_map(); build_close(&b);
        file=fopen("tests/native/cases.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/routines.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/stringfn.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/strlife.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/strbound.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        {
            const char *fixtures[]={"strindex","strops","strop255","arrays","arrayarg","chararr","records","variants","withrec","zerorec","sets","setshape","setargs","setabi","pointers","ptrargs","ordchar","heapmark","heapfree","pointerabi","absolute","absframe","rawmemory","nativeio","gotos","reals","realabi","formatout","strformat","valparse","readinput","endchar","sqrt","sqrtshadow","logexp","trig","trigshadow","random","randshadow","randomseed","filetypes","filetokens","params","diskfiles","diskblocks","disklife","diskwide","textdisk","textseek","textnumber","textformat","textrecover","textnest","texteof","textedges","textargs","capacity","textpath","textrename","textrens","stdinput","stdoutput","stdpred","keyonly","keybuffer","keybreak","keyhigh"};
            unsigned fixture;
            char path[80];
            for(fixture=0;fixture<sizeof(fixtures)/sizeof(fixtures[0]);++fixture) {
                unsigned before_allocations,required_allocations;
                snprintf(path,sizeof(path),"tests/native/%s.pas",fixtures[fixture]);
                file=fopen(path,"rb"); check(file!=NULL);
                n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
                check(source_import(&s,text,n,&d));
                before_allocations=allocation_calls;
                old=compile(&s,&b,&d); if(!old) fprintf(stderr,"fixture %s\n",fixtures[fixture]); check(old);
                required_allocations=allocation_calls-before_allocations;
                if(!strcmp(fixtures[fixture],"absolute")) check(b.globals_bytes==8);
                if(!strcmp(fixtures[fixture],"realabi")) check(b.globals_bytes==0);
                if(!strcmp(fixtures[fixture],"textedges")||!strcmp(fixtures[fixture],"capacity")) {
                    check(b.code_bytes>PAGE_SIZE&&0x4000u+b.length<=CC_STATIC_LIMIT&&b.pages[2]!=255);
                    check(cc.stage_page==255);
                }
                ordered_map(); build_close(&b);
                if(!strcmp(fixtures[fixture],"textedges")) {
                    for(i=0;i<required_allocations;++i) {
                        fail_after=(int)i;
                        check(!compile(&s,&b,&d)&&d.code==99);
                        check(pages_used()==1&&cc.stage_page==255);
                    }
                    fail_after=-1;
                }
            }
        }
        {
            const char *names[]={"stringabi","setabi","pointerabi"};
            unsigned abi;
            char path[80];
            for(abi=0;abi<sizeof(names)/sizeof(names[0]);++abi) {
                snprintf(path,sizeof(path),"tests/native/%s.pas",names[abi]);
                file=fopen(path,"rb"); check(file!=NULL);
                n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
                check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
                check(b.globals_bytes==0&&b.code_origin==GLOBAL_BASE);
                snprintf(path,sizeof(path),"build/%s.zpl",names[abi]);
                file=fopen(path,"rb"); check(file!=NULL);
                n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
                check(n==image_get(&b,0x4003));
                for(i=0;i<n;++i) check(image_get(&b,0x4004+(uint16_t)i)==text[i]);
                ordered_map(); build_close(&b);
            }
        }
        for(i=256;i<=257;++i) {
            unsigned member;
            n=(uint16_t)snprintf((char*)text,sizeof(text),"type t=(");
            for(member=0;member<i;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"v%u%s%s",member,member+1<i?",":"",member%10==9?"\n":"");
            n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"); var x:t; begin x:=v%u; writeln(ord(x),sizeof(t)) end.",i-1);
            check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
            check(b.globals_bytes==(i==256?1:2)); build_close(&b);
        }
        {
            unsigned member;
            n=(uint16_t)snprintf((char*)text,sizeof(text),"type t=(");
            for(member=0;member<513;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"v%u%s%s",member,member<512?",":"",member%10==9?"\n":"");
            snprintf((char*)text+n,sizeof(text)-n,"); begin end.");
            invalid((char*)text,99);
            n=(uint16_t)snprintf((char*)text,sizeof(text),"type\n");
            for(member=0;member<60;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"t%u=string[1];\n",member);
            snprintf((char*)text+n,sizeof(text)-n,"begin end.");
            invalid((char*)text,99);
        }
        {
            char left[128],right[128];
            memset(left,'a',126); left[126]='x'; left[127]=0;
            memcpy(right,left,128); right[126]='y';
            n=(uint16_t)snprintf((char*)text,sizeof(text),"var\n%s\n,\n%s\n:byte;\nbegin\n%s\n:=1;\n%s\n:=2 end.",left,right,left,right);
            check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
            check(b.globals_bytes==2); build_close(&b);
        }
        {
            unsigned arm;
            n=(uint16_t)snprintf((char*)text,sizeof(text),"begin\n");
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"case 1 of 1:\n");
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"end\n");
            snprintf((char*)text+n,sizeof(text)-n,"end.");
            invalid((char*)text,99);
            n=(uint16_t)snprintf((char*)text,sizeof(text),"begin case 1 of\n");
            for(arm=0;arm<600;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"%u:;\n",arm);
            snprintf((char*)text+n,sizeof(text)-n,"end end.");
            invalid((char*)text,99);
            n=0;
            for(arm=0;arm<256;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"procedure p%u; forward;\n",arm);
            snprintf((char*)text+n,sizeof(text)-n,"begin end.");
            invalid((char*)text,99);
            n=0;
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"procedure p%u;\n",arm);
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"begin end;\n");
            snprintf((char*)text+n,sizeof(text)-n,"begin end.");
            invalid((char*)text,99);
        }
    }
    {
        unsigned member;
        uint8_t text[1024];
        uint16_t n=(uint16_t)snprintf((char*)text,sizeof(text),"type b=array[1..127]of string[255];const x:b=(");
        for(member=0;member<127;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"%s''%s",member?",":"",member%10==9?"\n":"");
        snprintf((char*)text+n,sizeof(text)-n,");begin end.");
        invalid((char*)text,99); check(cc.stage_page==255);
    }
    source_close(&s); check(!pages_used());
    printf("PASS: %u host front-end, identity and allocation-failure checks\n",checks);
    return 0;
}
