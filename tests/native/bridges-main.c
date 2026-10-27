#include "harness.h"
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
Source s; Build b; Diagnostic d;
void test_complete(void);
void bridge_guard_marker(void) NATIVE;
int app_main(void) {
    int8_t capture;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/BRIDGEFRAME.PAS"));
    capture=open("H:/BRIDGES.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=43;
    execution_file("H:/BRIDGEFRAME.PAS",1,0,0);
    execution_file("H:/BRIDGEARGS.PAS",1,0,0);
    execution_file("H:/BRIDGERELOC.PAS",1,0,0);
    execution_file("H:/BRIDGEFILE.PAS",1,0,0);
    execution_file("H:/BRIDGEERROR.PAS",3,2,215);
    bridge_guard_marker();
    execution_file("H:/BRIDGEGUARD.PAS",3,0xff,424);
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
