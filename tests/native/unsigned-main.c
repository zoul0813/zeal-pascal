#include "compiler_units.h"
#include "library.h"
#include "image.h"
#include <zos_errors.h>
#include <zos_sys.h>
#include <zos_vfs.h>

volatile uint16_t test_passed,test_failed,test_index,test_stage,test_failures[64],test_diagnostics[128];
volatile uint8_t test_done;
Source s;Build b;Diagnostic d;
void test_complete(void);
static void check(uint8_t ok) {
    ++test_index;if(ok)++test_passed;
    else {if(test_failed<64){test_failures[test_failed]=test_index;test_diagnostics[2*test_failed]=d.code;test_diagnostics[2*test_failed+1]=d.start;}++test_failed;}
}
static uint8_t file_text(const char *path,const char *text) {
    int8_t fd;uint16_t n=(uint16_t)strlen(text);uint8_t ok;
    rm(path);fd=open(path,O_WRONLY|O_CREAT|O_TRUNC);if(fd<0)return 0;
    ok=!write(fd,text,&n)&&n==strlen(text);return !close(fd)&&ok;
}
static void closed(void) {
    uint8_t i,count=0;build_close(&b);
    for(i=0;i<code_required;++i)if(code_pages[i]!=255)++count;
    check(count==15&&code_pages[CODE_RUNTIME]==255&&pages_used()==16);
    for(i=0;i<CODE_BANK_LIMIT;++i)check(code_phase_pending[i]==255);
}
static uint8_t load(const char *text){return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);}
static void author_result(const char *path,const char *artifact,const char *text,uint8_t expected) {
    int8_t fd;check(source_new(&s,path));check(load(text));check(file_text(path,text));
    rm(artifact);fd=open(artifact,O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd>=0){uint8_t ok=compile_unit(&s,&b,&d,fd,0);check((expected?!ok&&d.code==expected:ok&&!d.code)&&!b.valid);check(!close(fd));}
    closed();source_close(&s);
}
static void author(const char *path,const char *artifact,const char *text){author_result(path,artifact,text,0);}
static void client(const char *path,const char *text,uint8_t full) {
    uint8_t status=255;
    check(source_new(&s,path));check(load(text));check(compile(&s,&b,&d));
    if(b.valid) {
        check(!!(far_get(b.pages[0],18)&NATIVE_SHARED_STACK)==!full);
        if(!full)check(b.pages[2]==255);
        check(save_build(&s,&b,0));
        {uint8_t ok=run_build(&s,&b);if(ram_result[26]!=1){d.code=ram_result[28];d.start=ram_result[30]|((uint16_t)ram_result[31]<<8);}check(ok&&ram_result[26]==1);}
        {uint8_t ok=run_build(&s,&b);if(ram_result[26]!=1){d.code=ram_result[28];d.start=ram_result[30]|((uint16_t)ram_result[31]<<8);}check(ok&&ram_result[26]==1);} /* Per activation, not per image. */
        check(save_build(&s,&b,0));check(run_saved(&s,&b,&status)&&status==0);
    }
    closed();source_close(&s);
}
static void reject(const char *text,uint8_t code) {
    check(source_new(&s,"H:/UNREJECT.PAS"));check(load(text));
    check(!compile(&s,&b,&d)&&d.code==code&&!b.valid);closed();source_close(&s);
}
static void failure(const char *text,uint8_t code) {
    check(source_new(&s,"H:/UNERROR.PAS"));check(load(text));check(compile(&s,&b,&d));
    if(b.valid)check(run_build(&s,&b)&&ram_result[26]==3&&ram_result[28]==code);
    closed();source_close(&s);
}
int app_main(void) {
    uint8_t status=255;int8_t capture;
    if(!console_open())return 1;
    memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=b.sources.page=255;
    test_stage=1;
    author("H:/WIDEMATH.PAS","H:/WIDEMATH.ZPU",
        "unit WideMath;interface const Max32=$FFFFFFFF;Max16=65535;type TValue=LongWord;\n"
        "TBox=record A:Word;B:TValue end;TIndex=32760..32775;TBytes=array[TIndex]of Byte;\n"
        "function Mix(X:TValue;N:Word):TValue;function Fixed:TValue;procedure Bump(var X:TValue);\n"
        "implementation function Mix(X:TValue;N:Word):TValue;begin Mix:=(X shl 16)or N end;\n"
        "function Fixed:TValue;begin Fixed:=$12345678 end;\n"
        "procedure Bump(var X:TValue);begin X:=X+1 end;end.");
    check(!rm("H:/WIDEMATH.PAS"));
    capture=open("H:/UNSIGNED.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);if(capture>=0)check(!swap(capture,DEV_STDOUT));
    test_stage=2;
    client("H:/UARITH.PAS","var W:Word;L:LongWord;Z:Integer;begin Z:=0;W:=65535;\n"
        "if(W div 257<>255)or(W mod 257<>0)or(W<32767)then Z:=1 div Z;W:=W+1;if W<>0 then Z:=1 div Z;\n"
        "L:=4294967295;L:=L+1;if L<>0 then Z:=1 div Z;L:=$80000000;\n"
        "if(L<=2147483647)or(L div 65536<>32768)or(L shr 31<>1)then Z:=1 div Z;\n"
        "L:=$12345678;if L*3<>$369D0368 then Z:=1 div Z;\n"
        "if(L and 65535<>$5678)or(L xor $FFFFFFFF<>$EDCBA987)then Z:=1 div Z;\n"
        "writeln('UNSIGNED-ARITH-OK')end.",1);
    client("H:/UCAST.PAS","var W:Word;L:LongWord;I,Z:Integer;begin Z:=0;I:=-1;W:=Word(I);L:=LongWord(I);\n"
        "if(W<>65535)or(L<>4294967295)then Z:=1 div Z;W:=Word($1234ABCD);if W<>$ABCD then Z:=1 div Z;\n"
        "L:=LongWord(65535)+1;if L<>65536 then Z:=1 div Z;\n"
        "if(Hi(L)<>1)or(Lo(L)<>0)or(not Odd(L+1))or(Swap(L)<>1)then Z:=1 div Z;\n"
        "writeln('UNSIGNED-CAST-OK')end.",1);
    client("H:/UCALL.PAS","uses WideMath;var L:LongWord;B:TBox;Z:Integer;\n"
        "function Twice(X:LongWord):LongWord;begin Twice:=X*2 end;\n"
        "begin Z:=0;L:=Mix(65535,65535);if L<>Max32 then Z:=1 div Z;B.A:=Max16;B.B:=L;\n"
        "Bump(L);if(L<>0)or(B.A<>65535)or(B.B<>Max32)then Z:=1 div Z;\n"
        "L:=Twice(Twice(123456789));if L<>493827156 then Z:=1 div Z;\n"
        "writeln('UNSIGNED-CALL-OK')end.",1);
    client("H:/UCONST.PAS","const A:array[1..2]of LongWord=(0,$FFFFFFFF);W:Word=65535;\n"
        "var L:LongWord;Z:Integer;S:string[12];begin Z:=0;L:=A[2];if(L<>4294967295)or(W<>65535)then Z:=1 div Z;\n"
        "Str(L:12,S);if S<>'  4294967295'then Z:=1 div Z;writeln('UNSIGNED-CONST-OK');writeln(W,' ',L);writeln(L:12)end.",1);
    client("H:/UINDEX.PAS","{$R+}uses WideMath;var A:TBytes;B:array[0..3]of Word;I:Word;Z,N:Integer;begin Z:=0;N:=0;\n"
        "for I:=32760 to 32775 do begin A[I]:=Byte(I);N:=N+1 end;if N<>16 then Z:=1 div Z;\n"
        "for I:=32760 to 32775 do if A[I]<>Byte(I)then Z:=1 div Z;\n"
        "for I:=0 to 3 do B[I]:=65535-I;if B[3]<>65532 then Z:=1 div Z;I:=40000;\n"
        "case I of 0..32767:Z:=1 div Z;40000:N:=42 end;if N<>42 then Z:=1 div Z;\n"
        "writeln('UNSIGNED-INDEX-OK')end.",1);
    client("H:/ULITE.PAS","var W:Word;begin W:=65535;W:=(W+2)*3;if(W<>3)or(W shr 1<>1)then halt end.",0);
    client("H:/UWORD.PAS","var W:Word;I,Z:Integer;C:Char;begin Z:=0;I:=-1;W:=5;\n"
        "if(W>=I)or(W>-1)or(-10 div W<>13105)then Z:=1 div Z;\n"
        "W:=65535;if(W shr 1<>32767)or(W shr 16<>0)or(W shl 16<>0)or(W*W<>1)then Z:=1 div Z;\n"
        "W:=65;C:=Chr(W);if(C<>'A')or(Char(W)<>'A')or(Word('A')<>65)then Z:=1 div Z;\n"
        "writeln('UNSIGNED-WORD-OK')end.",1);
    client("H:/URETURN.PAS","uses WideMath;var L:LongWord;Z:Integer;\n"
        "begin Z:=0;L:=Fixed;if(Hi(L)<>$1234)or(Lo(L)<>$5678)or(Hi(Swap(L))<>$5678)or(Lo(Swap(L))<>$1234)then Z:=1 div Z end.",0);
    client("H:/UHEX.PAS","{$R+}const Minus=$FFFF;var X:Byte absolute $C000;P:^Byte;I,Z:Integer;\n"
        "procedure Never;begin FillChar(X,$8000,0);Move(X,X,$8000);GetMem(P,$8000);StackPtr:=$F000;Port[1]:=$8000 end;\n"
        "begin Z:=0;I:=$FFFF;P:=Ptr($C000);if(I<>-1)or(Minus<>-1)or($FFFF div 2<>0)or($FFFF shr 1<>32767)then Z:=1 div Z;\n"
        "writeln('UNSIGNED-HEX-OK')end.",1);
    reject("begin Inline($12345678)end.",56);
    reject("var L:LongWord;B:Byte;begin B:=Mem[L]end.",23);
    reject("var L:LongWord;begin Mem[L]:=1 end.",23);
    reject("var L:LongWord;B:Byte;begin B:=Port[L]end.",23);
    reject("var W:Word;begin case W of 70000:halt end end.",46);
    reject("var I:Integer;begin case I of 100000:halt end end.",46);
    reject("var I:Integer;begin case I of 40000:halt end end.",46);
    reject("var W:Word;begin case W of -1:halt end end.",46);
    reject("const I:Integer=100000;begin end.",44);
    reject("const B:Byte=70000;begin end.",44);
    reject("const I:Integer=40000;begin end.",44);
    reject("var R:Real;W:Word;begin R:=[W]end.",44);
    reject("var I:Integer;begin I:=-40000 end.",56);
    reject("const L:LongWord=-1;begin end.",44);
    reject("const W:Word=-1;begin end.",44);
    reject("type T=-1..65535;begin end.",51);
    reject("type T=-32768..40000;begin end.",51);
    reject("var L:LongWord;begin for L:=0 to 2 do halt end.",30);
    reject("var L:LongWord;begin case L of 0:halt end end.",41);
    reject("var L:LongWord;begin Read(L)end.",66);
    reject("var W:Word;I:Integer;begin Val('1',W,I)end.",27);
    reject("var R:Real;W:Word;begin R:=Sin(W)end.",23);
    failure("{$R+}var A:array[0..1]of Integer;I:Word;begin I:=65535;A[I]:=0 end.",0x90);
    failure("{$R+}var W:Word;I:Integer;begin W:=40000;I:=W end.",0x91);
    failure("{$R+}var W:Word;L:LongWord;begin L:=65536;W:=L end.",0x91);
    failure("var L:LongWord;Z:Integer;begin Z:=0;L:=1;L:=L div Z end.",2);
    failure("{$R+}var W:Word;I:Integer;begin I:=-1;W:=I end.",0x91);
    failure("{$R+}type T=32760..32775;var W:T;begin W:=32775;W:=Succ(W)end.",0x91);
    failure("{$R+}var A:array[0..1]of Word;L:LongWord;begin L:=65536;A[1]:=L end.",0x91);
    test_stage=3;check(!rm("H:/WIDEMATH.ZPU"));check(!exec(EXEC_PRESERVE_PROGRAM,"H:/UCALL.COM",0,&status)&&status==0);
    if(capture>=0){check(!swap(capture,DEV_STDOUT));check(!close(capture));}
    check(pages_used()==15);console_close();test_done=255;test_complete();return 0;
}
