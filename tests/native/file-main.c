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

static void invalid(const char *text,uint8_t code) {
    uint8_t owned=pages_used();
    check(load(text));
    check(!compile(&s,&b,&d)&&d.code==code);
    check(pages_used()==owned);
}
int app_main(void) {
    int8_t capture;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/FILETYPES.PAS"));
    capture=open("H:/FILES.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=39;
    execution_file("H:/FILETYPES.PAS",1,0,0);
    invalid("var f,g:text;begin f:=g end.",54);
    invalid("var f:text;begin f:=1 end.",54);
    invalid("var f:text;begin writeln(f)end.",54);
    invalid("type f=file;procedure p(a:f);begin end;begin end.",67);
    invalid("function f:text;begin end;begin end.",67);
    invalid("type a=file;f=file of a;begin end.",68);
    invalid("const f:text='X';begin end.",61);
    invalid("type r=record f:text end;const a:r=(f:'X');begin end.",61);
    invalid("type a=file of integer;b=file of real;var f:b;\nprocedure p(var g:a);begin end;begin p(f)end.",44);
    invalid("type a=file of integer;b=file of byte;var f:b;\nprocedure p(var g:a);begin end;begin p(f)end.",44);
    invalid("type a=file of integer;b=file;var f:b;\nprocedure p(var g:a);begin end;begin p(f)end.",44);
    invalid("type b=file;var f:text;procedure p(var g:b);begin end;begin p(f)end.",44);
    invalid("type a=file of integer;c=array[1..2]of byte;b=file of c;var f:b;\nprocedure p(var g:a);begin end;begin p(f)end.",44);
    invalid("type a=file of string[10];b=file of string[10];var f:b;\nprocedure p(var g:a);begin end;begin p(f)end.",44);
    execution("type text=integer;var n:text;begin n:=7;if n<>7 then halt end.",1,0);
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
