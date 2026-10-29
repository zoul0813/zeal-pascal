#include "harness.h"
#include "compiler.h"

volatile uint16_t test_passed,test_failed,test_stage,test_failures[64],test_index;
volatile uint8_t test_done,test_hash[32];
Source s;Build b;Diagnostic d;
void test_complete(void);

int app_main(void) {
    static const char * const names[]={"H:/EXECALL.PAS","H:/EXENEXT.PAS","H:/EXERETRY.PAS",
        "H:/EXSTART.PAS","H:/EXCHAIN.PAS","H:/EXEFAIL.PAS","H:/EXECHECK.PAS"};
    CompileOptions options={0x8080,0xfbffu,COMPILE_PROGRAM};
    uint8_t i;
    if(!console_open())return 1;
    check(source_new(&s,names[0]));
    for(i=0;i<7;++i) {
        test_stage=90+i;strcpy(s.path,names[i]);options.start=i==1?0x8040:0x8080;
        options.kind=i==4?COMPILE_CHAIN:COMPILE_PROGRAM;
        check(load_source(&s,s.path,&d));
        check(compile_project(&s,0,&b,&d,0,&options));
        if(b.valid) {
            check(build_check(&s,&b)&&b.code_origin==options.start);
            check(i==1||cc.transfer_entry!=0);
            check(save_build(&s,&b,0));build_close(&b);
        }
    }
    test_stage=97;
    checked_error("var f:file;begin\nExecute(f)end.",21,"Execute");
    check(ram_result[27]==3);
    execution("var f:file;n:integer;begin {$I-}Execute(f);n:=IOResult;{$I+}\n"
        "if n<>21 then n:=1 div 0;\n"
        "if (Mem[$FE74]<>0)or(Mem[$FE75]<>0)then n:=1 div 0 end.",1,0);
    execution("var f:file;n:integer;procedure first;begin Assign(f,'H:/unused')end;\n"
        "procedure later;var t:Text;begin Assign(t,'H:/LATE.TXT');Rewrite(t);\n"
        "Writeln(t,42);Close(t);Reset(t);ReadLn(t,n);Close(t)end;\n"
        "begin first;later;if n<>42 then n:=1 div 0 end.",1,0);
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
