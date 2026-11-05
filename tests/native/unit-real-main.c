#include "compiler_units.h"
#include "library.h"
#include "output_transport.h"
#include <zos_sys.h>
#include <zos_vfs.h>

volatile uint16_t test_passed,test_failed,test_index,test_stage,test_failures[64];
volatile uint8_t test_done;
Source s;Build b;Diagnostic d;
void test_complete(void);
static void check(uint8_t ok) {
    ++test_index;
    if(ok)++test_passed;
    else {if(test_failed<64)test_failures[test_failed]=test_index;++test_failed;}
}
static uint8_t load(const char *text) {
    return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);
}
static void closed(void) {
    uint8_t i,count=0;build_close(&b);
    for(i=0;i<code_required;++i)if(code_pages[i]!=255)++count;
    check(count==15&&code_pages[CODE_RUNTIME]==255&&pages_used()==16);
    for(i=0;i<CODE_BANK_LIMIT;++i)check(code_phase_pending[i]==255);
}
static const char mixed[]=
    "unit Mixed;interface\n"
    "function Plain(X:Integer):Integer;\nfunction Scale(X:Real):Real;\n"
    "function CopyReal(X:Real):Real;\nfunction Transitive(X:Integer):Integer;\n"
    "implementation\nfunction Hidden(X:Real):Real;begin Hidden:=X*2 end;\n"
    "function Plain(X:Integer):Integer;begin Plain:=X+1 end;\n"
    "function Scale(X:Real):Real;begin Scale:=Hidden(X) end;\n"
    "function CopyReal(X:Real):Real;begin CopyReal:=X end;\n"
    "function Transitive(X:Integer):Integer;var R:Real;\n"
    "begin R:=Hidden(X);Transitive:=Trunc(R)end;end.";
static uint8_t author(void) {
    int8_t fd;uint8_t ok;uint16_t n=(uint16_t)strlen(mixed);
    check(source_new(&s,"H:/MIXED.PAS"));check(load(mixed));
    fd=open("H:/MIXED.PAS",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)return 0;
    check(!write(fd,mixed,&n)&&n==strlen(mixed));check(!close(fd));
    fd=open("H:/MIXED.ZPU",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)return 0;
    ok=compile_unit(&s,&b,&d,fd,0);check(ok&&!d.code&&!b.valid);
    check(!close(fd));closed();source_close(&s);return ok;
}
static void client(const char *path,const char *text) {
    uint8_t status=255;
    check(source_new(&s,path));check(load(text));check(compile(&s,&b,&d));
    if(b.valid) {
        check(save_build(&s,&b,0));check(run_build(&s,&b)&&ram_result[26]==1);
        check(run_saved(&s,&b,&status)&&status==0);
    }
    closed();source_close(&s);
}
int app_main(void) {
    uint8_t status;int8_t capture;
    if(!console_open())return 1;
    memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=b.sources.page=255;
    test_stage=1;if(!author())goto done;
    capture=open("H:/UNITREAL.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(!swap(capture,DEV_STDOUT));
    test_stage=2;
    client("H:/PLAIN.PAS","uses Mixed;begin if Plain(41)<>42 then halt;writeln('UNIT-INTEGER-OK')end.");
    client("H:/COPY.PAS","uses Mixed;var A,R:Real;P,Q,I:Integer;begin\n"
        "A:=1.5;R:=CopyReal(A);P:=Addr(A);Q:=Addr(R);\n"
        "for I:=0 to 5 do if Mem[P+I]<>Mem[Q+I]then halt;writeln('UNIT-COPY-OK')end.");
    client("H:/SCALE.PAS","uses Mixed;var A,R:Real;P,Q,I:Integer;begin\n"
        "A:=3.0;R:=Scale(1.5);P:=Addr(A);Q:=Addr(R);\n"
        "for I:=0 to 5 do if Mem[P+I]<>Mem[Q+I]then halt;writeln('UNIT-SCALE-OK')end.");
    client("H:/TRANSIT.PAS","uses Mixed;begin if Transitive(3)<>6 then halt;writeln('UNIT-TRANSITIVE-OK')end.");
    client("H:/UNUSED.PAS","uses Mixed;begin writeln('UNIT-UNUSED-OK')end.");
    test_stage=3;check(!rm("H:/MIXED.ZPU"));check(!rm("H:/MIXED.PAS"));
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/SCALE.COM",0,&status)&&status==0);
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/TRANSIT.COM",0,&status)&&status==0);
    if(capture>=0){check(!swap(capture,DEV_STDOUT));check(!close(capture));}
done:
    check(pages_used()==15);console_close();test_done=255;test_complete();return 0;
}
