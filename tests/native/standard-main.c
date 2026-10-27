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
    int8_t capture;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/STDOUTPUT.PAS"));
    capture=open("H:/STDS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=39;
    execution_file("H:/STDOUTPUT.PAS",1,0,0);
    execution_file("H:/STDINPUT.PAS",1,0,0);
    execution_file("H:/STDPRED.PAS",1,0,0);
    execution_file("H:/STDRECOVER.PAS",1,0,0);
    checked_error("begin close(Output)end.",0x22,"close");
    check(ram_result[27]==3&&ram_io_detail==0);
    checked_error("var a:array[1..2]of integer;begin {$I-}close(Output);{$I+}\nread(Input,a[1 div 0])end.",2,"read");
    check(ram_result[27]==2);
    execution("begin readln(Input)end.",5,0);
    execution("begin if eof then halt end.",5,0);
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
