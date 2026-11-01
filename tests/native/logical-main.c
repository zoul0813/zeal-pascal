#include "harness.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s;
Build b;
Diagnostic d;
void test_complete(void);

int app_main(void) {
    int8_t capture,input;
    uint8_t status,owned;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/KBDREAD.PAS"));
    capture=open("H:/KBDS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=39;
    execution_file("H:/KBDREAD.PAS",1,0,0);
    execution_file("H:/KBDBIND.PAS",1,0,0);
    execution_file("H:/KBDLIFE.PAS",1,0,0);
    execution_file("H:/KBDUPPER.PAS",1,0,0);
    execution_file("H:/KBDFAIL.PAS",3,0x10,40);
    check(seed("H:/KBDIN.TXT","x"));input=open("H:/KBDIN.TXT",O_RDONLY);check(input>=0);
    if(input>=0) {
        check(swap(input,DEV_STDIN)==0);status=255;owned=pages_used();
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/KBDFAIL.COM",0,&status)==0&&status==0x20);
        check(pages_used()==owned);check(swap(input,DEV_STDIN)==0);check(close(input)==0);
    }
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
