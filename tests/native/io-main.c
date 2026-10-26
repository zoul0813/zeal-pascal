#include "harness.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
uint8_t test_stop[3]={0x76,0x18,0xfd};
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
Source s;
Build b;
Diagnostic d;
void test_complete(void);

int app_main(void) {
    int8_t capture,denied;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/IOINPUT.PAS"));
    capture=open("H:/IO.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=39;
    execution_file("H:/IOINPUT.PAS",1,0,0);
    execution("{$I-}function unchecked:integer;var n:integer;begin readln(n);unchecked:=ioresult end;begin {$I+}writeln(unchecked:1)end.",1,0);
    checked_error("var n:integer;begin {$I-}readln(n);{$I+}\nwriteln('unreachable')end.",0x10,"writeln");
    check(ram_result[27]==3);
    checked_error("{$I+}function checked:integer;var n:integer;begin\nreadln(n);checked:=1 end;begin {$I-}writeln(checked)end.",0x10,"readln");
    check(ram_result[27]==3);
    checked_error("var n:integer;begin\nreadln(n)end.",0x10,"readln");
    check(ram_result[27]==3);
    test_stage=40;
    checked_error("begin {$I-}\nwriteln(1 div 0)end.",2,"writeln");
    check(ram_result[27]==2);
    checked_error("var a:array[1..2]of integer;begin {$I-,R+}\nreadln(a[3])end.",0x90,"readln");
    check(ram_result[27]==2);
    checked_error("var n:integer;begin {$I-}heapptr:=stackptr-80;\nreadln(n)end.",0xff,"readln");
    check(ram_result[27]==2);
    execution("function ioresult:integer;begin ioresult:=7 end;begin if ioresult<>7 then halt end.",1,0);
    execution("var ioresult:integer;begin ioresult:=7;if ioresult<>7 then halt end.",1,0);
    check(seed("H:/DENIED.TXT","read-only descriptor"));
    denied=open("H:/DENIED.TXT",O_RDONLY);check(denied>=0);
    if(denied>=0) {
        check(swap(denied,DEV_STDOUT)==0);test_stage=39;
        execution_file("H:/IOOUTPUT.PAS",1,0,0);
        checked_error("begin\nwriteln('unreachable')end.",0xf0,"writeln");
        check(ram_result[27]==3);
        check(swap(denied,DEV_STDOUT)==0);check(close(denied)==0);
    }
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
