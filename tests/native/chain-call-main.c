#include "harness.h"
#include "compiler.h"

volatile uint16_t test_passed,test_failed,test_stage,test_failures[64],test_index;
volatile uint8_t test_done,test_hash[32];
Source s;Build b;Diagnostic d;
void test_complete(void);

int app_main(void) {
    CompileOptions options={0x8040,0xfbffu,COMPILE_PROGRAM};
    uint8_t i;
    if(!console_open())return 1;
    check(source_new(&s,"H:/CALLER.PAS"));
    for(i=0;i<3;++i) {
        test_stage=80+i;
        if(i){strcpy(s.path,i==1?"H:/NEXT.PAS":"H:/FAILURE.PAS");options.kind=i==1?COMPILE_CHAIN:COMPILE_PROGRAM;}
        check(load_source(&s,s.path,&d));
        check(compile_project(&s,0,&b,&d,0,&options));
        if(b.valid) {
            check(build_check(&s,&b)&&b.code_origin==options.start);
            check(i==1?build_is_chain(&b):cc.chain_entry!=0);
            check(save_build(&s,&b,0));build_close(&b);
        }
    }
    test_stage=83;
    checked_error("var f:file;begin\nChain(f)end.",21,"Chain");
    check(ram_result[27]==3);
    execution("var f:file;n:integer;begin {$I-}Chain(f);n:=IOResult;{$I+}\n"
        "if n<>21 then n:=1 div 0;\n"
        "if (Mem[$FE74]<>0)or(Mem[$FE75]<>0)then n:=1 div 0 end.",1,0);
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
