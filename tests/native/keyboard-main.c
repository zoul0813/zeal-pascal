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
    execution_file("H:/KEYUFRAME.PAS",1,0,0);
    execution_file("H:/KEYUABI.PAS",1,0,0);
    execution_file("H:/KEYURAW.PAS",1,0,0);
    execution_file("H:/KEYUQUEUE.PAS",1,0,0);
    execution_file("H:/KEYUCACHED.PAS",5,0,57);
    execution_file("H:/KEYUREP.PAS",5,0,102);
    execution_file("H:/KEYUWHILE.PAS",5,0,30);
    execution_file("H:/KEYUFOR.PAS",5,0,43);
    execution_file("H:/KEYUGOTO.PAS",5,0,48);
    /* Inject an unavailable physical keyboard through supported descriptor
     * swapping. The checked poll aborts; I- returns its pending result. */
    check(load_source(&s,"H:/KEYUFAIL.PAS",&d)&&compile(&s,&b,&d)&&save_build(&s,&b,0));build_close(&b);
    check(load_source(&s,"H:/KEYUIGNORE.PAS",&d)&&compile(&s,&b,&d)&&save_build(&s,&b,0));build_close(&b);
    input=open("H:/KEYIN.TXT",O_RDONLY);check(input>=0);
    if(input>=0) {
        check(swap(input,DEV_STDIN)==0);owned=pages_used();status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/KEYUFAIL.COM",0,&status)==0&&status==0x20);
        check(pages_used()==owned);status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/KEYUIGNORE.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);check(swap(input,DEV_STDIN)==0);check(close(input)==0);
    }
    output("U-AFTER-BREAK-OK\r\n");
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
