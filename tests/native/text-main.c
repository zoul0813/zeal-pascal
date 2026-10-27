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
    check(source_new(&s,"H:/TEXTDISK.PAS"));
    capture=open("H:/TEXT.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=39;
    execution_file("H:/TEXTDISK.PAS",1,0,0);
    execution_file("H:/TEXTSEEK.PAS",1,0,0);
    execution_file("H:/TEXTNUMBER.PAS",1,0,0);
    execution_file("H:/TEXTFORMAT.PAS",1,0,0);
    execution_file("H:/TEXTRECOVER.PAS",1,0,0);
    execution_file("H:/TEXTNEST.PAS",1,0,0);
    execution_file("H:/TEXTEOF.PAS",1,0,0);
    execution_file("H:/TEXTEDGES.PAS",1,0,0);
    execution_file("H:/TEXTARGS.PAS",1,0,0);
    execution_file("H:/CAPACITY.PAS",1,0,0);
    check(chdir("H:/")==0);
    execution_file("H:/TEXTPATH.PAS",1,0,0);
    check(chdir("A:/")==0);
    checked_error("var f:text;n:integer;begin assign(f,'H:/BADNUM.TXT');\nreset(f);readln(f,n)end.",0x10,"readln");
    check(ram_result[27]==3&&ram_io_detail==0);
    checked_error("var f:text;begin assign(f,'H:/NESTIN.TXT');reset(f);\nwriteln(f,'X')end.",3,"writeln");
    check(ram_result[27]==3&&ram_io_detail==0);
    execution("var f:text;begin assign(f,'H:/AUTO.TXT');rewrite(f);\nwrite(f,'AUTO');halt end.",4,0);
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
