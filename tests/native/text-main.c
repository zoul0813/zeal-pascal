#include "harness.h"
#include "chunk.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
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
    if(chunk_select(5)) {
        test_stage=39;
        if(chunk_wants(1)) {
            execution_file("H:/TEXTDISK.PAS",1,0,0);
            execution_file("H:/TEXTSEEK.PAS",1,0,0);
        }
        if(chunk_wants(2)) {
            execution_file("H:/TEXTNUMBER.PAS",1,0,0);
        }
        if(chunk_wants(1)) {
            execution_file("H:/TEXTFORMAT.PAS",1,0,0);
        }
        if(chunk_wants(2)) {
            execution_file("H:/TEXTRECOVER.PAS",1,0,0);
        }
        if(chunk_wants(3)) {
            execution_file("H:/TEXTNEST.PAS",1,0,0);
            execution_file("H:/TEXTEOF.PAS",1,0,0);
        }
        if(chunk_wants(2)) {
            execution_file("H:/TEXTEDGES.PAS",1,0,0);
        }
        if(chunk_wants(3)) {
            execution_file("H:/TEXTARGS.PAS",1,0,0);
        }
        if(chunk_wants(4)) {
            execution_file("H:/CAPACITY.PAS",1,0,0);
        }
        if(chunk_wants(4)||chunk_wants(5)) {
            check(chdir("H:/")==0);
        }
        if(chunk_wants(4)) {
            execution_file("H:/TEXTPATH.PAS",1,0,0);
            test_groups[3]=1;
        }
        if(chunk_wants(5)) {
            execution_file("H:/TEXTRENAME.PAS",1,0,0);
            execution_file("H:/TEXTRENS.PAS",1,0,0);
        }
        if(chunk_wants(4)||chunk_wants(5)) {
            check(chdir("A:/")==0);
        }
        if(chunk_wants(2)) {
            strcpy(s.path,"H:/TEXTRENS.PAS");
            checked_error("var f:text;n:integer;begin assign(f,'H:/BADNUM.TXT');\nreset(f);readln(f,n)end.",0x10,"readln");
            check(ram_result[27]==3&&ram_io_detail==0);
            test_groups[1]=1;
        }
        if(chunk_wants(5)) {
            strcpy(s.path,"H:/TEXTRENS.PAS");
            checked_error("var f:text;begin assign(f,'H:/RNERROR.OLD');rewrite(f);\nwrite(f,'ERROR');close(f);rename(f,'H:/RNNEW.TXT')end.",0x20,"rename");
            check(ram_result[27]==3&&ram_io_detail==ERR_ALREADY_EXIST);
            test_groups[4]=1;
        }
        if(chunk_wants(3)) {
            strcpy(s.path,"H:/TEXTRENS.PAS");
            checked_error("var f:text;begin assign(f,'H:/NESTIN.TXT');reset(f);\nwriteln(f,'X')end.",3,"writeln");
            check(ram_result[27]==3&&ram_io_detail==0);
            test_groups[2]=1;
        }
        if(chunk_wants(1)) {
            strcpy(s.path,"H:/TEXTRENS.PAS");
            execution("var f:text;begin assign(f,'H:/AUTO.TXT');rewrite(f);\nwrite(f,'AUTO');halt end.",4,0);
            test_groups[0]=1;
        }
    }
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
