#include "harness.h"
#include "compiler_job.h"
#include "library.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s;Build b;Diagnostic d;
void test_complete(void);
/* Keep this focused target's checks in its test bank; the full corpus harness
 * also contains unrelated resident execution helpers. */
void check(uint8_t ok) {
    ++test_index;
    if(ok)++test_passed;
    else {if(test_failed<64)test_failures[test_failed]=test_index;++test_failed;}
}
uint8_t load(const char *text) {return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);}
static CompileFailure failure;
static uint8_t expected[32];
static uint8_t request(uint8_t mode,const CompileOptions *options) {
    return compile_request(&s,0,&b,&failure,mode,options,s.path);
}
static void closed(void) {build_close(&b);check(pages_used()==code_required+1);}
int app_main(void) {
    Source before,item;Sources buffers;CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};
    uint8_t hash[32],status=255,owned;int8_t fd;uint16_t pc,position;
    if(!console_open())return 1;
    check(source_new(&s,"H:/JOB.PAS"));
    fd=open("H:/JOBS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);if(fd>=0)check(!swap(fd,DEV_STDOUT));
    test_stage=80;
    check(load("program Hello;begin writeln('JOB-OK')end."));s.cursor=8;s.previous=3;s.dirty=1;before=s;
    check(compile(&s,&b,&d));memcpy(expected,b.id,32);closed();
    compiler_allocating=COMPILER_ALLOC_WORKER;owned=page_new(PAGE_COMPILER);compiler_allocating=COMPILER_ALLOC_NONE;
    check(owned!=255);check(code_phase_park());check(request(0,0));
    check(job_zero(compiler_owned,8)&&job_zero(compiler_parent_owned,8));check(b.valid&&!memcmp(b.id,expected,32)&&!memcmp(&s,&before,sizeof(s)));
    check(!*output_recovery()&&build_check(&s,&b));owned=pages_used();
    check(run_build(&s,&b)&&ram_result[26]==1);check(pages_used()==owned);
    check(code_phase_restore());closed();
    test_stage=81;
    check(request(1,&options));check(*output_recovery());check(save_build(&s,&b,0));
    check(!*output_recovery()&&!memcmp(&s,&before,sizeof(s)));
    check(run_saved(&s,&b,&status)&&status==0&&b.valid);check(code_phase_restore());closed();
    options.kind=COMPILE_CHAIN;check(request(2,&options));check(build_is_chain(&b)&&*output_recovery());
    check(save_build(&s,&b,0)&&!*output_recovery());closed();options.kind=COMPILE_PROGRAM;
    test_stage=82;
    check(load("program Include;\n{$I PART.INC}\nbegin writeln(answer)end."));before=s;
    check(request(1,&options));check(b.sources.count==2&&sources_get(&b.sources,1,&item)&&item.page==255);
    check(save_build(&s,&b,0)&&!*output_recovery()&&!strcmp(output_path(),"H:/JOB.COM"));
    check(build_check(&s,&b)&&!memcmp(&s,&before,sizeof(s)));
    pc=96+far_get(b.map_page,84)+((uint16_t)far_get(b.map_page,85)<<8);
    pc=far_get(b.map_page,pc)|((uint16_t)far_get(b.map_page,pc+1)<<8);
    check(build_locate(&s,&b,pc,&position));closed();
    test_stage=83;
    check(load("program Bad;   \nvar n:integer;   \nbegin n:=;end."));before=s;
    check(!compile(&s,&b,&d));position=d.start;
    check(!request(1,&options)&&failure.diagnostic.code==d.code&&failure.diagnostic.start==position);
    check(!strcmp(failure.path,s.path)&&failure.has_hash&&!*output_recovery()&&pages_used()==code_required+1&&!memcmp(&s,&before,sizeof(s)));
    check(load("program Include;\n{$I BAD.INC}\nbegin end."));
    check(!request(0,0)&&failure.diagnostic.code<101&&!strcmp(failure.path,"H:/BAD.INC")&&failure.has_hash&&pages_used()==code_required+1);
    check(load("program Missing;\n{$I MISSING.INC}\nbegin end."));
    check(!request(1,&options)&&failure.diagnostic.code==90&&!strcmp(failure.path,"H:/MISSING.INC")&&!failure.has_hash&&!*output_recovery()&&pages_used()==code_required+1);
    test_stage=84;
    check(load("program Override;\n{$I PART.INC}\nbegin writeln(answer)end."));
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    check(source_new(&item,"H:/PART.INC"));check(source_import(&item,(const uint8_t*)"const answer=;",14,&d));
    source_hash(&item,hash);check(sources_borrow(&buffers,&item,hash)==1);owned=pages_used();
    check(!compile_request(&s,&buffers,&b,&failure,0,0,item.path));
    check(failure.diagnostic.code<101&&!strcmp(failure.path,item.path)&&failure.has_hash&&!memcmp(failure.hash,hash,32)&&pages_used()==owned);
    check(sources_close(&buffers));source_close(&item);check(pages_used()==code_required+1);
    test_stage=85;
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    memset(&item,0,sizeof(item));item.page=255;strcpy(item.path,"H:/DISK.PAS");owned=pages_used();
    check(compile_request(&item,&buffers,&b,&failure,0,0,s.path));
    check(b.valid&&sources_get(&b.sources,0,&item)&&item.page!=255&&!sources_borrowed(&b.sources,0));
    check(build_check(&item,&b));build_close(&b);check(pages_used()==owned);check(sources_close(&buffers));
    test_stage=86;
    check(load_source(&s,"H:/LARGE.PAS",&d));before=s;
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    options.start=0x8040;owned=pages_used();
    check(compile_request(&s,&buffers,&b,&failure,1,&options,s.path));
    check(b.valid&&b.code_bytes>PAGE_SIZE&&build_check(&s,&b));
    check(!memcmp(&s,&before,sizeof(s))&&sources_ready(&buffers,0));
    check(save_build(&s,&b,0)&&!*output_recovery());build_close(&b);check(pages_used()==owned);
    check(sources_close(&buffers));
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
