#include "harness.h"
#include "sources.h"
volatile uint16_t test_passed,test_failed,test_stage,test_index,test_pc,test_io_pc;
volatile uint16_t test_failures[64];
volatile uint8_t test_done;
Source s;Build b;Diagnostic d;
void test_complete(void);
int app_main(void) {
    Source located;zos_stat_t info;uint8_t file,before[7],after[7];uint16_t position;
    int8_t capture;uint16_t at;uint8_t j;
    if(!console_open())return 1;
    check(source_new(&s,"H:/project/main.pas"));
    check(load_source(&s,s.path,&d));
    capture=open("H:/LOOKUP.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=55;test_machine(before);
    check(compile(&s,&b,&d)&&save_build(&s,&b,0));
    check(stat("H:/project/main.ZMP",&info)==ERR_NO_SUCH_ENTRY);
    check(run_build(&s,&b)&&ram_result[26]==3&&ram_result[28]==2);
    test_pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
    check(build_location(&s,&b,test_pc,&file,&position)&&file==1&&position==0);
    check(sources_get(&b.sources,file,&located)&&!strcmp(located.path,"H:/project/parts/fail.inc"));
    build_close(&b);check(rm("H:/project/main.COM")==0);
    /* Fresh F can reconstruct its position from source without COM or ZMP. */
    check(compile(&s,&b,&d));
    check(build_location(&s,&b,test_pc,&file,&position)&&file==1&&position==0);
    check(stat("H:/project/main.COM",&info)==ERR_NO_SUCH_ENTRY);
    check(!build_location(&s,&b,0xffff,&file,&position));
    build_close(&b);
    /* Twenty tiny includes must not consume twenty physical source pages. */
    check(load_source(&s,"H:/project/many.pas",&d));
    check(compile(&s,&b,&d)&&b.sources.count==21&&b.sources.text_page==255);
    check(run_build(&s,&b)&&ram_result[26]==1);build_close(&b);
    /* TP3 initializes typed constants at compilation, not each Memory Run. */
    check(load("const counter:integer=0;begin counter:=counter+1;writeln(counter)end."));
    check(compile(&s,&b,&d));
    check(run_build(&s,&b)&&ram_result[26]==1);
    check(run_build(&s,&b)&&ram_result[26]==1&&build_check(&s,&b));
    /* The final record must terminate lookup without wrapping a 16-bit sum.
     * All entries cover PC0, so PC1 scans the entire near-full map. */
    check(buffer_reserve(b.map_page,65528u));
    for(at=0;at<65528u;at+=MAP_RECORD_BYTES)
        for(j=0;j<MAP_RECORD_BYTES;++j)buffer_set(b.map_page,at+j,j==2?1:0);
    b.map_length=65528u;b.versions[3]=page_versions[b.map_page];
    check(!build_location(&s,&b,1,&file,&position));
    test_machine(after);check(!memcmp(before,after,7));
    build_close(&b);check(pages_used()==code_required+1);
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
