#include "harness.h"
#include "sources.h"
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
Source s;Build b;Diagnostic d;
void test_complete(void);
static void included_error(void) {
    uint8_t file,status,owned;uint16_t pc,position;Source source;
    check(load_source(&s,"H:/INLSRC.PAS",&d));s.dirty=1;
    check(compile(&s,&b,&d));if(!b.valid)return;
    check(save_build(&s,&b));owned=pages_used();
    check(run_build(&s,&b)&&ram_result[26]==3&&ram_result[28]==2);
    pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
    check(build_location(&s,&b,pc,&file,&position)&&file==1&&position==0);
    check(sources_get(&b.sources,file,&source)&&!strcmp(source.path,"H:/source-parts/inlerror.inc"));
    status=255;check(run_saved(&s,&b,&status)&&status==2);
    check(pages_used()==owned&&s.dirty&&build_check(&s,&b));build_close(&b);
    check(pages_used()==code_required+1);
    check(load_source(&s,"H:/INLBAD.PAS",&d));owned=pages_used();s.dirty=1;
    check(!compile(&s,&b,&d)&&d.code==74&&d.start==7);
    check(!strcmp(compile_error_path(),"H:/source-parts/inlbad.inc")&&compile_error_hash()!=0);
    check(s.dirty&&pages_used()==owned);
}
int app_main(void) {
    int8_t capture;
    if(!console_open())return 1;
    check(source_new(&s,"H:/INLBYTES.PAS"));
    capture=open("H:/INLINE.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=53;
    execution_file("H:/INLBYTES.PAS",1,0,0);
    execution_file("H:/INLADDR.PAS",1,0,0);
    execution_file("H:/INLFRAME.PAS",1,0,0);
    execution_file("H:/INLSTD.PAS",1,0,0);
    execution_file("H:/INLPAGES.PAS",1,0,0);
    execution_file("H:/INLERROR.PAS",3,2,6);
    included_error();
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
