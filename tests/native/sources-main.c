#include "harness.h"
#include "sources.h"
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
Source s;Build b;Diagnostic d;
void test_complete(void);
static void included_error(void) {
    uint8_t owned,file,status,before[7],after[7];uint16_t pc,position;
    Source source;
    check(load_source(&s,"H:/SRCERROR.PAS",&d));s.dirty=1;
    test_machine(before);check(compile(&s,&b,&d));test_machine(after);
    check(!memcmp(before,after,7));if(!b.valid)return;
    check(save_build(&s,&b));owned=pages_used();
    check(run_build(&s,&b));check(ram_result[26]==3&&ram_result[28]==2);
    pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
    check(build_location(&s,&b,pc,&file,&position)&&file==1&&position==0);
    check(sources_get(&b.sources,file,&source)&&!strcmp(source.path,"H:/source-parts/run.inc"));
    check(!build_locate(&s,&b,pc,&position));
    status=255;check(run_saved(&s,&b,&status)&&status==2);
    check(pages_used()==owned&&s.dirty&&build_check(&s,&b));build_close(&b);
    check(pages_used()==code_required+1);
    check(load_source(&s,"H:/SRCBAD.PAS",&d));s.dirty=1;owned=pages_used();
    check(!compile(&s,&b,&d)&&d.code==56&&d.start==13);
    check(!strcmp(compile_error_path(),"H:/source-parts/bad.inc"));
    check(s.dirty&&pages_used()==owned);
}
static void unsaved_override(void) {
    Source root,work;Sources buffers;uint8_t status,owned;
    uint8_t hash[32],after[32];
    uint8_t ok;
    ok=read_block(&root,"H:/SRCOVER.PAS",&d);check(ok);if(!ok)return;
    ok=read_block(&work,"H:/source-parts/work.inc",&d);check(ok);
    if(!ok){source_close(&root);return;}
    check(source_import(&work,(const uint8_t*)"const answer=42;\n",17,&d));work.dirty=1;
    ok=sources_new(&buffers);check(ok);
    if(!ok){source_close(&root);source_close(&work);return;}
    check(sources_take(&buffers,&root,&root)==0);
    check(sources_take(&buffers,&work,&work)==1);source_hash(&work,hash);
    check(compile_project(&root,&buffers,&b,&d,0));if(!b.valid){sources_close(&buffers);return;}
    check(save_build(&root,&b));owned=pages_used();
    check(run_build(&root,&b)&&ram_result[26]==1);status=255;
    check(run_saved(&root,&b,&status)&&status==0);
    source_hash(&work,after);check(work.dirty&&!memcmp(hash,after,32)&&pages_used()==owned);
    check(source_replace(&work,work.length,work.length,(const uint8_t*)" ",1));
    check(!build_ready(&root,&b));build_close(&b);
    check(sources_take(&buffers,&work,&work)==1);
    check(compile_project(&root,&buffers,&b,&d,0)&&build_ready(&root,&b));build_close(&b);
    check(sources_close(&buffers)&&pages_used()==code_required+1);
}
int app_main(void) {
    int8_t capture;
    if(!console_open())return 1;
    check(source_new(&s,"H:/SRCMAIN.PAS"));
    capture=open("H:/SOURCES.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=51;
    execution_file("H:/SRCMAIN.PAS",1,0,0);
    execution_file("H:/SRCSTEPS.PAS",1,0,0);
    execution_file("H:/SRCINIT.PAS",1,0,0);
    included_error();unsaved_override();
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
