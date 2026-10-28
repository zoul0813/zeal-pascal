#include "harness.h"
#include "compiler.h"
#include "saved_map.h"
#include "image.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
Source s;Build b;Diagnostic d;
void test_complete(void);

static uint16_t word(uint16_t at) {return image_get(&b,at)|((uint16_t)image_get(&b,at+1)<<8);}
int app_main(void) {
    CompileOptions options={0x9000,0xfbffu,COMPILE_PROGRAM};
    uint8_t i,owned,status,header[160];uint16_t pc,at,position;
    NativeImage identity;SavedLocation found;
    if(!console_open())return 1;
    check(source_new(&s,"H:/BASE.PAS"));
    test_stage=70;
    check(load_source(&s,s.path,&d));check(compile_project(&s,0,&b,&d,0,&options));
    check(b.valid&&!build_is_chain(&b));check(save_build(&s,&b,0));build_close(&b);
    strcpy(s.path,"H:/CHAIN.PAS");check(load_source(&s,s.path,&d));s.dirty=1;
    options.kind=COMPILE_CHAIN;
    check(compile_project(&s,0,&b,&d,0,&options));
    if(b.valid) {
        check(build_is_chain(&b)&&b.length==96+b.code_bytes);
        check(word(6)==96&&word(8)==1&&word(10)==0x9000&&word(12)==b.code_bytes);
        check(word(14)==GLOBAL_BASE&&word(16)==b.globals_bytes&&word(18)==0xfbffu);
        for(i=0;i<96;++i)header[i]=image_get(&b,i);
        check(native_header(header,1,&identity)==0&&!memcmp(identity.id,b.id,32));
        owned=pages_used();check(build_ready(&s,&b)&&build_check(&s,&b));
        check(!run_build(&s,&b)&&platform_error==ERR_NOT_SUPPORTED&&pages_used()==owned);
        status=55;check(!run_saved(&s,&b,&status)&&platform_error==ERR_NOT_SUPPORTED&&status==55);
        check(save_build(&s,&b,0)&&!*output_recovery()&&s.dirty);
        check(!strcmp(output_path(),"H:/CHAIN.CHN"));
        at=96+far_get(b.map_page,84)+((uint16_t)far_get(b.map_page,85)<<8);
        pc=far_get(b.map_page,at)|((uint16_t)far_get(b.map_page,at+1)<<8);
        position=far_get(b.map_page,at+8)|((uint16_t)far_get(b.map_page,at+9)<<8);
        check(pc>=CC_ACTIVATION_BYTES);
        check(saved_location_kind(s.path,0,pc,&found,1)&&!strcmp(found.path,s.path)&&found.position==position);
        check(!memcmp(found.hash,b.source_hash,32)&&pages_used()==owned);
        check(!saved_location(s.path,0,pc,&found));
        check(build_check(&s,&b));build_close(&b);
    }
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
