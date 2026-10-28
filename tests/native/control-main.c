#include "harness.h"
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
Source s;Build b;Diagnostic d;
void test_complete(void);
int app_main(void) {
    int8_t capture,input;
    uint8_t status,owned,full_only;
    if(!console_open())return 1;
    input=open("H:/FULL.ONLY",O_RDONLY);full_only=input>=0;
    if(input>=0)close(input);
    check(source_new(&s,"H:/CTLSTATE.PAS"));
    capture=open("H:/CONTROL.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=39;
    if(!full_only) {
    execution_file("H:/CTLSTATE.PAS",1,0,0);
    execution_file("H:/CTLMINUS.PAS",1,0,0);
    execution_file("H:/CTLBREAK.PAS",5,0,46);
    execution_file("H:/CTLTRM.PAS",5,0,42);
    execution_file("H:/CTLPAUSE.PAS",1,0,0);
    execution_file("H:/CTLSTOP.PAS",5,0,103);
    output("C-AFTER-BREAK-OK\r\n");
    }
    execution_file("H:/CTLFULL.PAS",1,0,0);
    if(!full_only) {
    execution_file("H:/CTLQUEUED.PAS",1,0,0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    execution_file("H:/CTLSTDOUT.PAS",1,0,0);
    execution_file("H:/CTLBOUNDARY.PAS",1,0,0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    check(load_source(&s,"H:/CTLFAIL.PAS",&d)&&compile(&s,&b,&d)&&save_build(&s,&b));build_close(&b);
    check(load_source(&s,"H:/CTLIGNORE.PAS",&d)&&compile(&s,&b,&d)&&save_build(&s,&b));build_close(&b);
    check(seed("H:/CONTROL.IN","x"));input=open("H:/CONTROL.IN",O_RDONLY);check(input>=0);
    if(input>=0) {
        check(swap(input,DEV_STDIN)==0);owned=pages_used();status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/CTLSTATE.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/CTLFAIL.COM",0,&status)==0&&status==0xf0);
        check(pages_used()==owned);status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/CTLIGNORE.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);check(swap(input,DEV_STDIN)==0);check(close(input)==0);
    }
    }
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
