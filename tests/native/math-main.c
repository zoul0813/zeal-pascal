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
    int8_t capture;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/SQRT.PAS"));
    capture=open("H:/MATH.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=35;
    execution_file("H:/SQRT.PAS",1,0,0);
    execution_file("H:/SQRTSHADOW.PAS",1,0,0);
    execution_file("H:/LOGEXP.PAS",1,0,0);
    execution_file("H:/TRIG.PAS",1,0,0);
    execution_file("H:/TRIGSHADOW.PAS",1,0,0);
    execution_file("H:/RANDOM.PAS",1,0,0);
    execution_file("H:/RANDSHADOW.PAS",1,0,0);
    execution_file("H:/RANDOMSEED.PAS",1,0,0);
    execution_file("H:/RANDBAD.PAS",3,0x91,13);
    execution_file("H:/RANDGUARD.PAS",3,0xff,41);
    execution_file("H:/LOGBAD.PAS",3,4,33);
    execution_file("H:/EXPBAD.PAS",3,1,36);
    test_stage=36;
    checked_error("begin {$R-}\nwriteln(sqrt(-1))end.",3,"writeln");
    checked_error("begin\nwriteln(sqrt(-32768))end.",3,"writeln");
    checked_error("var r:real;begin r:=-0.25;\nwriteln(sqrt(r))end.",3,"writeln");
    checked_error("function negative:integer;begin negative:=-1 end;begin\nwriteln(sqrt(negative))end.",3,"writeln");
    checked_error("var r:real;begin heapptr:=stackptr-128;\nr:=sqrt(4)end.",0xff,"r:=sqrt");
    checked_error("begin\nwriteln(ln(0))end.",4,"writeln");
    checked_error("begin {$R-}\nwriteln(ln(-1))end.",4,"writeln");
    checked_error("var r:real;begin r:=-0.25;\nwriteln(ln(r))end.",4,"writeln");
    checked_error("begin\nwriteln(exp(89))end.",1,"writeln");
    checked_error("begin\nwriteln(exp(128))end.",1,"writeln");
    checked_error("var r:real;begin heapptr:=stackptr-128;\nr:=ln(2)end.",0xff,"r:=ln");
    checked_error("var r:real;begin heapptr:=stackptr-128;\nr:=exp(1)end.",0xff,"r:=exp");
    checked_error("var r:real;begin heapptr:=stackptr-128;\nr:=sin(1)end.",0xff,"r:=sin");
    checked_error("var r:real;begin heapptr:=stackptr-128;\nr:=cos(1)end.",0xff,"r:=cos");
    checked_error("var r:real;begin heapptr:=stackptr-128;\nr:=arctan(1)end.",0xff,"r:=arctan");
    checked_error("begin {$R-}\nwriteln(random(0))end.",0x91,"writeln");
    checked_error("begin {$R-}\nwriteln(random(-1))end.",0x91,"writeln");
    checked_error("var n:integer;begin n:=-32768;\nwriteln(random(n))end.",0x91,"writeln");
    checked_error("function bound:integer;begin bound:=0 end;begin\nwriteln(random(bound))end.",0x91,"writeln");
    checked_error("var n:integer;begin heapptr:=stackptr-72;\nn:=random(1)end.",0xff,"n:=random");
    checked_error("var r:real;begin heapptr:=stackptr-72;\nr:=random end.",0xff,"r:=random");
    checked_error("begin heapptr:=stackptr-72;\nrandomize end.",0xff,"randomize");
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
