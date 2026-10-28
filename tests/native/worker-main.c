#include "harness.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
Source s;
Build b;
Diagnostic d;
void test_complete(void);
static uint8_t probe_saved;

static void job(const char *name,uint8_t valid,uint8_t kind) {
    uint8_t owned=pages_used(),retained,before[7],after[7],i;
    uint16_t position,pc;
    uint32_t revision;
    check(load_source(&s,name,&d));check(compile(&s,&b,&d));
    if(!b.valid) return;
    retained=pages_used();
    check(b.globals_bytes==0);
    if(!probe_saved) {check(save_build(&s,&b));probe_saved=1;}
    s.dirty=1;revision=s.revision;
    test_machine(before);
    check(run_build(&s,&b)==valid);
    test_machine(after);
    check(!memcmp(before,after,7));
    check(ram_result[26]==kind);check(ram_result[81]==1);
    if(name[10]=='5'||name[10]=='6') check(platform_error==2);
    check(s.dirty&&s.revision==revision);
    if(kind==3) {
        pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
        check(ram_result[27]==2&&ram_result[28]==2);
        check(build_locate(&s,&b,pc,&position));
        check(far_get(s.page,position)=='n'&&far_get(s.page,position+1)==':');
    }
    check(pages_used()==retained);check(build_check(&s,&b));
    for(i=32;i<64;++i) check(page_role(i)!=PAGE_WORKER);
    build_close(&b);check(pages_used()==owned);
}
int app_main(void) {
    int8_t capture,borrowed;
    uint16_t n;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/WORKERS0.PAS"));
    capture=open("H:/WORKERS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture==2);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    borrowed=open("H:/BORROW.DAT",O_RDWR);check(borrowed==3);
    test_stage=40;code_hash((uint8_t*)test_hash);
    job("H:/WORKERS0.PAS",1,1);
    __asm di __endasm;
    job("H:/WORKERS0.PAS",1,1);
    __asm ei __endasm;
    job("H:/WORKERS1.PAS",0,1);
    job("H:/WORKERS2.PAS",0,1);
    job("H:/WORKERS3.PAS",1,3);
    job("H:/WORKERS4.PAS",1,4);
    job("H:/WORKERS5.PAS",1,3);
    job("H:/WORKERS6.PAS",0,1);
    job("H:/WORKERS0.PAS",1,1);
    if(borrowed>=0) {
        n=1;console_scratch[0]='P';
        check(write(borrowed,console_scratch,&n)==0&&n==1);check(close(borrowed)==0);
    }
    code_hash((uint8_t*)b.id);check(!memcmp(b.id,(const uint8_t*)test_hash,32));
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
