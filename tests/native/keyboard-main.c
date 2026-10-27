#include "harness.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
Source s;
Build b;
Diagnostic d;
void test_complete(void);

int app_main(void) {
    int8_t capture,input;
    uint8_t status,owned;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/KEYONLY.PAS"));
    capture=open("H:/KEYS.TXT",O_WRONLY|O_CREAT|O_TRUNC); check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=39;
    execution_file("H:/KEYONLY.PAS",1,0,0);
    /* A regular file cannot serve keyboard mode/status calls. Execute the
     * saved child directly so parent console preparation does not preempt the
     * actual runtime failure; restore both borrowed descriptors afterwards. */
    check(seed("H:/KEYIN.TXT","x"));input=open("H:/KEYIN.TXT",O_RDONLY);check(input>=0);
    if(input>=0) {
        check(swap(input,DEV_STDIN)==0);status=255;owned=pages_used();
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/KEYONLY.COM",0,&status)==0&&status==0x20);
        check(pages_used()==owned);check(swap(input,DEV_STDIN)==0);check(close(input)==0);
    }
    execution_file("H:/KEYBUFFER.PAS",1,0,0);
    execution_file("H:/KEYHIGH.PAS",1,0,0);
    execution_file("H:/KEYBREAK.PAS",5,0,30);
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
