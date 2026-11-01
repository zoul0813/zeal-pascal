#include "harness.h"

/* Focused console acceptance uses the same resident harness and real RAM/COM
 * execution as the full corpus, without replaying unrelated language groups. */
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s;
Build b;
Diagnostic d;
void test_complete(void);

int app_main(void) {
    int8_t capture;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/READINPUT.PAS"));
    capture=open("H:/READ.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=39;
    execution_file("H:/READINPUT.PAS",1,0,0);
    execution_file("H:/ENDCHAR.PAS",1,0,0);
    execution_file("H:/CONSOLE.PAS",1,0,0);
    execution_file("H:/INPUTEOF.PAS",1,0,0);
    check(load_source(&s,"H:/BUFLENBAD.PAS",&d));
    execute_loaded(3,0x91,67);
    check(load_source(&s,"H:/BREAKINPUT.PAS",&d));
    execute_loaded(5,0,45);
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
