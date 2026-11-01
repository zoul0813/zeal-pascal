#include "harness.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s;
Build b;
Diagnostic d;
void test_complete(void);

static void saved(const char *tail) {
    char owned_tail[129];
    const char *arguments[1];
    uint8_t status=255,owned=pages_used(),before[7],after[7];
    strcpy(owned_tail,tail);
    strcpy((char*)console_scratch,"H:/PARAMS.COM");
    test_machine(before);
    /* Prepare arguments after the snapshot so SDCC does not keep their
     * pointer in a temporary spill across only the first machine sample. */
    arguments[0]=owned_tail;
    check(exec(EXEC_PRESERVE_PROGRAM,(char*)console_scratch,arguments,&status)==0);
    test_machine(after);
    check(status==0);check(!memcmp(before,after,7));
    check(pages_used()==owned);check(build_check(&s,&b));
}
int app_main(void) {
    int8_t capture,denied;
    uint16_t i;
    char tail[129];
    if(!console_open()) return 1;
    check(source_new(&s,"H:/PARAMS.PAS"));
    capture=open("H:/PARAMS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=40;
    check(load_source(&s,"H:/PARAMS.PAS",&d));check(compile(&s,&b,&d));
    if(b.valid) {
        check(save_build(&s,&b,0));
        check(run_build(&s,&b)&&ram_result[26]==1);
        saved("");saved(" \t ");saved("  one\tTWO  three-four \t");
        saved("\"two words\"\tMiXeD");
        for(i=0;i<127;++i) tail[i]='x';tail[127]=0;saved(tail);
        for(i=0;i<127;++i) tail[i]=i&1?' ':'x';saved(tail);
        memset(tail,' ',127);saved(tail);
        /* The application rejects128 bytes before any Pascal statement. */
        {
            const char *arguments[1];uint8_t status=255;
            memset(tail,'x',128);tail[128]=0;arguments[0]=tail;
            strcpy((char*)console_scratch,"H:/PARAMS.COM");
            check(exec(EXEC_PRESERVE_PROGRAM,(char*)console_scratch,arguments,&status)==0&&status==1);
        }
        /* A saved program's command tail cannot become a later RAM run's. */
        check(run_build(&s,&b)&&ram_result[26]==1);
        check(build_check(&s,&b));build_close(&b);
    }
    checked_error("begin heapptr:=stackptr-200;\nwriteln(paramstr(1))end.",0xff,"writeln");
    check(seed("H:/PARAMDEN.TXT","read-only descriptor"));
    denied=open("H:/PARAMDEN.TXT",O_RDONLY);check(denied>=0);
    if(denied>=0) {
        check(swap(denied,DEV_STDOUT)==0);
        execution("var n:integer;s:string[127];begin {$I-}write('blocked');\n"
                  "n:=paramcount;s:=paramstr(1);if(ioresult<>$f0)or(n<>0)or(s<>'')then halt end.",1,0);
        check(swap(denied,DEV_STDOUT)==0);check(close(denied)==0);
    }
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
