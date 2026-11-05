#include "compiler.h"
#include "compiler_disk.h"
#include "library.h"
#include "optional_runtime.h"
#include "output_transport.h"
#include <zos_errors.h>
#include <zos_sys.h>

volatile uint16_t test_passed,test_failed,test_index,test_stage,test_failures[64];
volatile uint8_t test_done;
Source s;Build b;Diagnostic d;
void test_complete(void);
extern const uint8_t real_patch_start[];
static void check(uint8_t ok) {
    ++test_index;if(ok)++test_passed;
    else {if(test_failed<64)test_failures[test_failed]=test_index;++test_failed;}
}
static uint8_t load(const char *text) {
    return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);
}
static uint8_t cached(void) {
    uint8_t i,n=0;for(i=0;i<code_required;++i)if(code_pages[i]!=255)++n;return n;
}
static void closed(void) {
    uint8_t i;build_close(&b);check(cached()==14&&code_pages[CODE_RUNTIME]==255);
    for(i=0;i<CODE_BANK_LIMIT;++i)check(code_phase_pending[i]==255);
    check(pages_used()==15);
}
static const char integer[]="var N,C:Integer;\nbegin Val('42',N,C);\nif(N<>42)or(C<>0)then halt end.";
static const char real[]="var R:Real;\nbegin R:=1.5;R:=R*4;\nif R<>6.0 then halt;writeln('REAL-RETRY-OK')end.";
static void corrupt_case(uint8_t kind) {
    uint8_t page;uint16_t base=(uint16_t)real_patch_start&0x3fffu;Source before;
    check(load(real));before=s;
    check(code_phase_real());page=code_pages[CODE_RUNTIME];check(page>=32&&page<64);
    if(page<32||page>=64)return;
    if(kind==0)far_set(page,base,'?');
    if(kind==1){far_set(page,base+4,255);far_set(page,base+5,255);}
    if(kind==2){far_set(page,base+6,0);far_set(page,base+7,0);}
    if(kind==3){far_set(page,base+8,255);far_set(page,base+9,255);}
    if(kind==4){far_set(page,base+12,far_get(page,base+12)|1);far_set(page,base+13,0);far_set(page,base+14,0);}
    check(!compile(&s,&b,&d)&&d.code==COMPILE_PLATFORM_FAILURE&&!b.valid);
    check(!memcmp(&s,&before,sizeof(s)));closed();
    /* The mutated data page was released. Reload the original immutable
     * component from disk and prove a successful retry and execution. */
    check(compile(&s,&b,&d));check(run_build(&s,&b)&&ram_result[26]==1);closed();
}
/* Exhaust the actual compile path at each small allocator budget. At least
 * one budget must reach the late Real load and fail with platform status. */
static void compile_pressure(uint8_t disk_mode) {
    uint8_t pressure[32],count,budget,page,saw_real_failure=0;
    Source before;
    check(load(real));before=s;
    for(budget=0;budget<8;++budget) {
        count=0;
        while(count<32&&!palloc(pressure+count))++count;
        check(count>=budget);
        for(page=0;page<budget&&count;++page)check(!pfree(pressure[--count]));
        compiler_disk_requested=disk_mode;
        if(!compile(&s,&b,&d)&&d.code==COMPILE_PLATFORM_FAILURE&&cc.real_needed)
            saw_real_failure=1;
        compiler_disk_requested=0;
        build_close(&b);
        while(count)check(!pfree(pressure[--count]));
        check(!memcmp(&s,&before,sizeof(s)));closed();
    }
    check(saw_real_failure);
    compiler_disk_requested=disk_mode;check(compile(&s,&b,&d));compiler_disk_requested=0;
    closed();
}
int app_main(void) {
    uint8_t pressure[32],count=0,i;Source before;Build previous;
    if(!console_open())return 1;
    memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=b.sources.page=255;
    check(source_new(&s,"H:/PROFILE.PAS"));check(cached()==14&&code_pages[CODE_RUNTIME]==255);
    check(load(integer));check(compile(&s,&b,&d));before=s;previous=b;
    test_stage=1;
    while(count<32&&!palloc(pressure+count))++count;
    check(count<32);check(!code_phase_real()&&platform_error!=0);
    check(!memcmp(&s,&before,sizeof(s))&&!memcmp(&b,&previous,sizeof(b)));
    check(code_pages[CODE_RUNTIME]==255&&code_phase_pending[CODE_RUNTIME]==255);
    while(count)check(pfree(pressure[--count])==0);
    closed();
    test_stage=2;
    for(i=0;i<5;++i)corrupt_case(i);
    compile_pressure(0);compile_pressure(1);
    test_stage=3;
    check(load(real));check(compile(&s,&b,&d));check(save_build(&s,&b,0));
    check(run_build(&s,&b)&&ram_result[26]==1);closed();
    source_close(&s);check(pages_used()==14);console_close();test_done=255;test_complete();return 0;
}
