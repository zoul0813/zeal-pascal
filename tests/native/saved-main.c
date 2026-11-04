#include "harness.h"
#include "sources.h"
#include "compiler.h"
#include "compiler_disk.h"
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
    /* Permissive lengths and V- formals must never overrun String slots. */
    check(load_source(&s,"H:/project/strings.pas",&d));
    check(compile(&s,&b,&d)&&run_build(&s,&b)&&ram_result[26]==1);build_close(&b);
    check(load_source(&s,"H:/project/casts.pas",&d));
    check(compile(&s,&b,&d)&&run_build(&s,&b)&&ram_result[26]==1);build_close(&b);
    /* The lookup compiler uses disk capacity, with no output preparation. */
    check(load_source(&s,"H:/project/deep.pas",&d));
    {CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};
     compiler_disk_requested=1;check(compile_project(&s,&b,&d,1,&options));compiler_disk_requested=0;
     check(b.valid==2&&save_build(&s,&b,0));
     {uint8_t status=255;check(run_saved(&s,&b,&status)&&status==0);}}
    build_close(&b);
    check(load_source(&s,"H:/project/diskscope.pas",&d));
    check(compile(&s,&b,&d)&&run_build(&s,&b)&&ram_result[26]==1);
    build_close(&b);
    {CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};uint8_t status=255;
     compiler_disk_requested=1;check(compile_project(&s,&b,&d,1,&options));compiler_disk_requested=0;
     check(save_build(&s,&b,0)&&run_saved(&s,&b,&status)&&status==0);build_close(&b);}
    /* A checked I/O failure must not retain the old IOCALL landing. */
    check(load_source(&s,"H:/project/ioframe.pas",&d));
    check(compile(&s,&b,&d));
    check(run_build(&s,&b)&&ram_result[26]==3);
    check(run_build(&s,&b)&&ram_result[26]==1);build_close(&b);
    /* TP3 initializes typed constants at compilation, not each Memory Run. */
    check(load("const counter:integer=0;begin counter:=counter+1;writeln(counter)end."));
    check(compile(&s,&b,&d));
    check(run_build(&s,&b)&&ram_result[26]==1);
    /* Leftover runtime internals must reset without resetting counter. */
    far_set(b.pages[0],RT_IOSTATUS-0x4000u,9);
    far_set(b.pages[0],cc_runtime_private(RP_LINE_VALID)-0x4000u,7);
    far_set(b.pages[0],cc_runtime_private(RP_LINE_EOF)-0x4000u,1);
    far_set(b.pages[0],cc_runtime_private(RP_LINE_REMAINING)-0x4000u,9);
    far_set(b.pages[0],RT_HEAPPTR-0x4000u,0xf0);far_set(b.pages[0],RT_HEAPPTR-0x4000u+1,0xff);
    b.versions[0]=page_versions[b.pages[0]];
    check(run_build(&s,&b)&&ram_result[26]==1&&build_check(&s,&b));
    check(image_get(&b,RT_IOSTATUS-0x4000u)==0&&
        image_get(&b,cc_runtime_private(RP_LINE_VALID)-0x4000u)==0&&
        image_get(&b,cc_runtime_private(RP_LINE_EOF)-0x4000u)==0&&
        image_get(&b,cc_runtime_private(RP_LINE_REMAINING)-0x4000u)==0);
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
