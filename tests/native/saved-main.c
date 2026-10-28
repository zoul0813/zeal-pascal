#include "harness.h"
#include "saved_map.h"
volatile uint16_t test_passed,test_failed,test_stage,test_index,test_pc,test_io_pc;
volatile uint16_t test_failures[64];
volatile uint8_t test_done,test_hash[32];
Source s;Build b;Diagnostic d;
void test_complete(void);
int app_main(void) {
    Sources edits;Source part;SavedLocation found,previous;
    uint8_t status,owned,held[32],count=0,page,file,hash[32],before[7],after[7];
    uint16_t position;
    int8_t capture;
    if(!console_open())return 1;
    memset(&found,0,sizeof(found));
    check(source_new(&s,"H:/project/main.pas"));
    check(load_source(&s,s.path,&d));
    capture=open("H:/LOOKUP.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=55;test_machine(before);
    check(compile(&s,&b,&d));check(save_build(&s,&b));check(run_build(&s,&b));
    check(ram_result[26]==3&&ram_result[28]==2);
    test_pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
    check(build_location(&s,&b,test_pc,&file,&position));check(file==1&&position==0);
    build_close(&b);owned=pages_used();
    /* No compiler cache exists when either saved execution or F reads it. */
    status=255;check(exec(EXEC_PRESERVE_PROGRAM,"H:/project/main.COM",0,&status)==0&&status==2);
    check(pages_used()==owned);
    check(saved_location(s.path,0,test_pc,&found));
    check(!strcmp(found.path,"H:/project/parts/fail.inc")&&found.position==0);
    previous=found;
    check(seed("H:/project/parts/fail.inc","n:=2 div 0;\n"));
    check(!saved_location(s.path,0,test_pc,&found));check(!memcmp(&found,&previous,sizeof(found)));
    check(seed("H:/project/parts/fail.inc","n:=1 div 0;\n"));
    check(saved_location(s.path,0,test_pc,&found));check(pages_used()==owned);
    check(sources_new(&edits));check(source_replace(&s,0,0,(const uint8_t*)"{dirty}\n",8));s.dirty=1;
    check(sources_take(&edits,&s,&s)>=0);source_hash(&s,hash);
    check(!saved_location(s.path,&edits,test_pc,&found));
    check(s.dirty&&s.cursor==8&&sources_ready(&edits,0));
    check(source_replace(&s,0,8,0,0));check(sources_take(&edits,&s,&s)>=0);
    check(saved_location(s.path,&edits,test_pc,&found));
    check(source_new(&part,"H:/project/parts/fail.inc"));
    check(source_import(&part,(const uint8_t*)"n:=1 div 0;\n",12,&d));part.dirty=1;
    check(sources_take(&edits,&part,&part)>=0);
    check(seed(part.path,"n:=2 div 0;\n"));
    check(saved_location(s.path,&edits,test_pc,&found)); /* Matching unsaved include overrides disk. */
    check(!saved_location(s.path,0,test_pc,&found));
    check(part.dirty&&sources_ready(&edits,0)&&sources_ready(&edits,1));
    check(seed(part.path,"n:=1 div 0;\n"));
    owned=pages_used();
    while((page=page_new(PAGE_RUN))!=255&&count<32)held[count++]=page;
    check(!saved_location(s.path,&edits,test_pc,&found));check(sources_ready(&edits,0)&&sources_ready(&edits,1));
    while(count)check(page_drop(held[--count]));
    check(pages_used()==owned&&saved_location(s.path,&edits,test_pc,&found));
    check(!saved_location(s.path,&edits,0xffff,&found));
    check(sources_close(&edits));check(pages_used()==code_required);
    check(source_new(&s,"H:/ioerr.pas"));check(load_source(&s,s.path,&d));
    check(compile(&s,&b,&d));check(save_build(&s,&b));check(run_build(&s,&b));
    check(ram_result[26]==3&&ram_result[27]==3&&ram_result[28]==1);
    test_io_pc=ram_result[30]|((uint16_t)ram_result[31]<<8);build_close(&b);
    status=255;check(exec(EXEC_PRESERVE_PROGRAM,"H:/ioerr.COM",0,&status)==0&&status==1);
    check(saved_location(s.path,0,test_io_pc,&found));check(!strcmp(found.path,s.path));
    source_close(&s);check(pages_used()==code_required);
    test_machine(after);check(!memcmp(before,after,7));
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    test_stage=6;test_done=255;test_complete();return 0;
}
