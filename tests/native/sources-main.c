#include "harness.h"
#include "source/catalog.h"
typedef char native_build_handle_size[sizeof(Build)==54?1:-1];
typedef char native_catalog_handle_size[sizeof(Sources)==13?1:-1];
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s;Build b;Diagnostic d;
void test_complete(void);
static void included_error(void) {
    uint8_t owned,file,status,before[7],after[7];uint16_t pc,position;
    Source source;
    check(load_source(&s,"H:/SRCERROR.PAS",&d));s.dirty=1;
    test_machine(before);check(compile(&s,&b,&d));test_machine(after);
    check(!memcmp(before,after,7));if(!b.valid)return;
    check(b.sources.text_page==255&&sources_valid(&b.sources)&&sources_ready(&b.sources,0));
    check(save_build(&s,&b,0));owned=pages_used();
    check(run_build(&s,&b));check(ram_result[26]==3&&ram_result[28]==2);
    pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
    check(build_location(&s,&b,pc,&file,&position)&&file==1&&position==0);
    check(sources_get(&b.sources,file,&source)&&!strcmp(source.path,"H:/source-parts/run.inc"));
    check(!build_locate(&s,&b,pc,&position));
    status=255;check(run_saved(&s,&b,&status)&&status==2);
    check(pages_used()==code_required+1&&s.dirty&&!b.valid);build_close(&b);
    check(pages_used()==code_required+1);
    check(load_source(&s,"H:/SRCBAD.PAS",&d));s.dirty=1;owned=pages_used();
    check(!compile(&s,&b,&d)&&d.code==56&&d.start==13);
    check(!strcmp(compile_error_path(),"H:/source-parts/bad.inc"));
    check(s.dirty&&pages_used()==owned);
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
    included_error();
    /* The following case owns its own root/work buffers. Do not retain an
     * unrelated previous source against the sixteen-bank RAM budget. */
    source_close(&s);
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
