#include "harness.h"
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s; Build b; Diagnostic d;
void test_complete(void);
static void memory_overlays(void) {
    const char *cases[]={
        "program Test;\noverlay procedure One; begin end; begin One end.",
        "overlay function One:integer; begin One:=1 end; begin writeln(One) end.",
        "procedure Outer;\noverlay procedure Inner; begin end; begin Inner end; begin Outer end."};
    uint8_t i;
    for(i=0;i<3;++i) {
        check(load(cases[i]));
        check(!compile(&s,&b,&d)&&d.code==77);
        check(d.start==(uint16_t)(strstr(cases[i],"overlay")-cases[i]));
        check(!b.valid&&pages_used()==code_required+1);
    }
}
int app_main(void) {
    int8_t capture;
    if(!console_open())return 1;
    check(source_new(&s,"H:/INDEXMODES.PAS"));
    memory_overlays();
    capture=open("H:/LANGUAGE.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=49;
    execution_file("H:/INDEXMODES.PAS",1,0,0);
    execution_file("H:/INDEXWRAP.PAS",1,0,0);
    execution_file("H:/INDEXRANGE.PAS",3,0x90,109);
    execution_file("H:/INDEXRSMALL.PAS",3,0x90,109);
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
