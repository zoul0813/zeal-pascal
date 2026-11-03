#include "harness.h"
#include "chunk.h"
#include "compiler.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s;Build b;Diagnostic d;
void test_complete(void);
extern const uint8_t trampoline_start[],trampoline_end[];

/* The supervisor's immutable return template must survive every operation.
 * Stop before copying a damaged template into another RAM child. The host
 * qualification also binds these bytes to the assembled trampoline asset. */
static uint8_t trampoline_copy[358];
static void immutable(uint8_t phase) {
    uint8_t ok=(uint16_t)(trampoline_end-trampoline_start)==sizeof(trampoline_copy)&&!memcmp(trampoline_start,trampoline_copy,sizeof(trampoline_copy));
    check(ok);
    if(!ok) {test_stage=100+phase;test_done=255;test_complete();}
}

static void put16(uint16_t at,uint16_t value) {
    far_set(b.pages[at/PAGE_SIZE],at%PAGE_SIZE,(uint8_t)value);
    ++at;far_set(b.pages[at/PAGE_SIZE],at%PAGE_SIZE,(uint8_t)(value>>8));
}
static void changed_header(uint16_t at,uint16_t value) {
    uint8_t i;
    put16(at,value);
    /* Deliberate corruption reaches generated-runtime checks; update only the
     * test owner's versions, never add a product content-validation bypass. */
    for(i=0;i<3;++i)if(b.pages[i]!=255)b.versions[i]=page_versions[b.pages[i]];
    check(build_check(&s,&b));
}
static void run(void) {
    uint8_t owned=pages_used(),status=255,before[7],after[7];
    uint16_t at,pc,position,found;uint8_t i;
    for(i=0;i<3;++i)if(b.pages[i]==255)++owned;
    test_machine(before);
    check(run_build(&s,&b)&&ram_result[26]==1);
    immutable(1);
    test_machine(after);check(!memcmp(before,after,7));
    check(pages_used()==owned&&build_check(&s,&b));
    immutable(2);
    check(save_build(&s,&b,0));immutable(3);
    at=0;
    pc=far_get(b.map_page,at)|((uint16_t)far_get(b.map_page,at+1)<<8);
    position=far_get(b.map_page,at+6)|((uint16_t)far_get(b.map_page,at+7)<<8);
    check(run_saved(&s,&b,&status)&&status==0);immutable(4);
    test_machine(after);check(!memcmp(before,after,7));
    check(!b.valid&&b.map_page==255&&pages_used()==code_required+1);
    check(compile(&s,&b,&d)&&build_locate(&s,&b,pc,&found));immutable(5);
    check(found==position);build_close(&b);
    check(pages_used()==code_required+1);

}
#include "save-path-native.h"
int app_main(void) {
    const uint16_t starts[]={0,0x9000,0xbfff,0xc000,0xc001,0xd100,0,0};
    const uint16_t ends[]={0xfbff,0xd000,0xefff,0xefff,0xefff,0xfbff,0xcfff,0xfbfe};
    const char *paths[]={"H:/OBASE.PAS","H:/OLOW.PAS","H:/OBFFF.PAS","H:/OC000.PAS",
        "H:/OC001.PAS","H:/OD100.PAS","H:/OMIN.PAS","H:/OODD.PAS","H:/OMAX.PAS"};
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};CompileLimits limits;
    uint16_t i,at,bytes=0,globals=0,origin;
    uint8_t gap,status,owned;
    int8_t capture;
    if((uint16_t)(trampoline_end-trampoline_start)!=sizeof(trampoline_copy))return 1;
    memcpy(trampoline_copy,trampoline_start,sizeof(trampoline_copy));
    if(!console_open())return 1;
    capture=open("H:/PATHONLY.TEST",O_RDONLY);
    if(capture>=0) {
        check(!close(capture));native_save_paths();
        test_stage=6;test_done=255;test_complete();return 0;
    }
    check(source_new(&s,"H:/OBASE.PAS"));
    capture=open("H:/OPTIONS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    if(chunk_select(6)) {
        test_stage=60;
        for(i=0;i<9;++i) {
            if(!chunk_wants(1+i/3))continue;
            strcpy(s.path,paths[i]);check(load_source(&s,s.path,&d));s.dirty=1;
            options.start=i<8?starts[i]:0;options.end=i<8?ends[i]:0xfbff;
            if(i==8) {
                check(compile_limits(globals,bytes,&options,&limits));options.start=limits.maximum_start;
            }
            check(compile_project(&s,&b,&d,0,&options));
            immutable(6);
            if(!b.valid)continue;
            bytes=b.code_bytes;globals=b.globals_bytes;
            origin=options.start?options.start:GLOBAL_BASE+globals;
            check(b.code_origin==origin&&image_get(&b,30)==(uint8_t)options.end&&image_get(&b,31)==(uint8_t)(options.end>>8));
            gap=1;for(at=GLOBAL_BASE+globals-0x4000u;at<origin-0x4000u;++at)if(image_get(&b,at))gap=0;
            check(gap);run();check(s.dirty);build_close(&b);
            check(pages_used()==code_required+1);
            if(i%3==2)test_groups[i/3]=1;
        }
        test_stage=61;
        for(i=0;i<6;++i) {
            if(!chunk_wants(i==0||i==2?4:i==1||i==5?5:6))continue;
            strcpy(s.path,"H:/OBAD.PAS");check(load_source(&s,s.path,&d));check(compile(&s,&b,&d));
            if(!b.valid)continue;
            /* Re-authenticate deliberate malformed images so execution tests the
             * runtime's own pre-entry checks rather than an image hash rejection. */
            changed_header(i==1||i==5?32:i>=3?28:30,i==0||i>=4?0xffff:i==1?2047:i==2?0xcffe:0xf401);
            owned=pages_used();check(run_build(&s,&b)&&ram_result[26]==3&&ram_result[28]==255);
            check(pages_used()==owned&&build_check(&s,&b));
            check(save_build(&s,&b,0));status=0;
            check(run_saved(&s,&b,&status)&&status==255);
            check(!b.valid&&pages_used()==code_required+1);build_close(&b);
            if(i==2)test_groups[3]=1;
            else if(i==5)test_groups[4]=1;
            else if(i==4)test_groups[5]=1;
        }
    }
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
