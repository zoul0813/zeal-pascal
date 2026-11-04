#include "harness.h"
#include "chunk.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
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
static uint16_t position(const char *text) {
    uint16_t i,j,n=(uint16_t)strlen(text);
    for(i=0;i+n<=s.length;++i) {
        for(j=0;j<n&&far_get(s.page,i+j)==(uint8_t)text[j];++j) {}
        if(j==n) return i;
    }
    return 0xffff;
}
/* The independent probe consumes only handles owned by the RAM child.
 * Saved programs have no parent resource service; their ordinary cleanup
 * remains covered by the lifetime corpus and exact external disk bytes. */
static void cleanup_error(const char *path,const char *where) {
    check(load_source(&s,path,&d));
    execute_loaded(3,0x99,position(where));
    check(ram_result[27]==3&&ram_io_detail==6&&ram_cleanup_detail==0);
}
int app_main(void) {
    int8_t capture;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/FILETYPES.PAS"));
    capture=open("H:/FILES.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    if(chunk_select(8)) {
        test_stage=39;
        if(chunk_wants(1)) {
            execution_file("H:/FILETYPES.PAS",1,0,0);
            execution_file("H:/FILETOKENS.PAS",1,0,0);
        }
        if(chunk_wants(2)) {
            execution_file("H:/DISKFILES.PAS",1,0,0);
            execution_file("H:/DISKBLOCKS.PAS",1,0,0);
        }
        if(chunk_wants(3)) {
            execution_file("H:/DISKLIFE.PAS",1,0,0);
        }
        if(chunk_wants(2)) {
            execution_file("H:/DISKWIDE.PAS",1,0,0);
            test_groups[1]=1;
        }
        if(chunk_wants(3)) {
            strcpy(s.path,"H:/DISKWIDE.PAS");
            execution("var f:file of integer;i:integer;begin\nassign(f,'H:/HALTIO.DAT');rewrite(f);\ni:=88;write(f,i);halt end.",4,0);
            checked_error("var f:file of integer;i:integer;begin\nassign(f,'H:/FAULTIO.DAT');rewrite(f);\ni:=99;write(f,i);\ni:=1 div 0 end.",2,"i:=1");
            check(ram_result[27]==2&&ram_io_detail==0);
            test_groups[2]=1;
        }
        if(chunk_wants(4)) {
            strcpy(s.path,"H:/DISKWIDE.PAS");
            checked_error("var f:file of integer;begin\nassign(f,'H:/MISSING.DAT');\nreset(f)end.",1,"reset");
            check(ram_result[27]==3&&ram_io_detail==4);
            checked_error("type z=record end;var f:file of z;\nbegin assign(f,'H:/ZERO.DAT');\nrewrite(f)end.",0x90,"rewrite");
            check(ram_result[27]==3&&ram_io_detail==0);
            test_groups[3]=1;
        }
        if(chunk_wants(1)) {
            strcpy(s.path,"H:/DISKWIDE.PAS");
            invalid("var f,g:text;begin f:=g end.",54);
            invalid("var f:text;begin f:=1 end.",54);
            invalid("var f:text;begin writeln(1+f)end.",54);
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
            test_groups[0]=1;
        }
        if(chunk_wants(5)) {
            execution_file("H:/DISKABI.PAS",1,0,0);
            check(load_source(&s,"H:/DISKFAIL.PAS",&d));
            execute_loaded(3,2,position("i:=1 div 0"));
            check(ram_result[27]==2&&ram_io_detail==0&&ram_cleanup_detail==6);
            test_groups[4]=1;
        }
        if(chunk_wants(6)) {
            strcpy(s.path,"H:/DISKFAIL.PAS");
            execution("begin stackptr:=$C800;halt end.",4,0);
            execution("begin stackptr:=$C800;exit end.",1,0);
            execution("begin stackptr:=$C800 end.",1,0);
            /* C services use their own module stack. Check the remaining
             * Pascal gate/job headroom and the independent C800 floor. */
            checked_error("var f:file of integer;begin assign(f,'H:/BADSTACK.DAT');\nstackptr:=$C7FF end.",0xff,"stackptr");
            checked_error("var f:file of integer;pad:^byte;begin assign(f,'H:/BADSTACK.DAT');\n"
                "if hi(heapptr)<$C8 then getmem(pad,$C800-heapptr);\nstackptr:=heapptr+128;\nrewrite(f)end.",0xff,"rewrite");
            execution("var f:file of integer;i:integer;pad:^byte;begin assign(f,'H:/LOWHALT.DAT');\n"
                "rewrite(f);i:=81;write(f,i);\nif hi(heapptr)<$C8 then getmem(pad,$C800-heapptr);\n"
                "stackptr:=heapptr+1024;halt end.",4,0);
            execution("var f:file of integer;i:integer;pad:^byte;begin assign(f,'H:/LOWEXIT.DAT');\n"
                "rewrite(f);i:=82;write(f,i);\nif hi(heapptr)<$C8 then getmem(pad,$C800-heapptr);\n"
                "stackptr:=heapptr+1024;exit end.",1,0);
            execution("var f:file of integer;i:integer;pad:^byte;begin assign(f,'H:/LOWEND.DAT');\n"
                "rewrite(f);i:=83;write(f,i);\nif hi(heapptr)<$C8 then getmem(pad,$C800-heapptr);\n"
                "stackptr:=heapptr+1024 end.",1,0);
            test_groups[5]=1;
        }
        test_stage=40;
        if(chunk_wants(7)) {
            cleanup_error("H:/SCOPE.PAS","close(local)");
            cleanup_error("H:/EXIT.PAS","close(local)");
            execution_file("H:/IGNORE.PAS",1,0,0);
            check(ram_cleanup_detail==0);
            execution_file("H:/EXITOFF.PAS",1,0,0);
            check(ram_cleanup_detail==0);
            test_groups[6]=1;
        }
        if(chunk_wants(8)) {
            cleanup_error("H:/DISPOSE.PAS","close(p^.f)");
            cleanup_error("H:/FREE.PAS","close(p^.f)");
            cleanup_error("H:/RELEASE.PAS","close(p^.f)");
            cleanup_error("H:/FINISH.PAS","close(f)");
            cleanup_error("H:/HALT.PAS","close(f)");
            test_groups[7]=1;
        }
    }
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
