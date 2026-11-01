#include "harness.h"
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s; Build b; Diagnostic d;
void test_complete(void);
int app_main(void) {
    int8_t capture,input;
    uint8_t status,owned;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/HOOKOUT.PAS"));
    capture=open("H:/HOOKS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=45;
    execution_file("H:/HOOKOUT.PAS",1,0,0);
    execution_file("H:/HOOKMULTI.PAS",1,0,0);
    execution_file("H:/HOOKAUX.PAS",1,0,0);
    execution_file("H:/HOOKSTATUS.PAS",1,0,0);
    execution_file("H:/HOOKUSR.PAS",1,0,0);
    execution_file("H:/HOOKECHO.PAS",1,0,0);
    execution_file("H:/HOOKPURE.PAS",1,0,0);
    /* The saved callback must work even when native stdin cannot poll keys.
     * Execute directly so parent keyboard preparation cannot preempt it. */
    check(seed("H:/HOOKIN.TXT","x"));input=open("H:/HOOKIN.TXT",O_RDONLY);check(input>=0);
    if(input>=0) {
        check(swap(input,DEV_STDIN)==0);owned=pages_used();status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/HOOKPURE.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);check(swap(input,DEV_STDIN)==0);check(close(input)==0);
    }
    execution_file("H:/HOOKERROR.PAS",3,2,88);
    execution_file("H:/HOOKIOERR.PAS",3,0x20,121);
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
