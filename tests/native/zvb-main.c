#include "compiler_units.h"
#include "library.h"
#include "image.h"
#include <zos_errors.h>
#include <zos_sys.h>
#include <zos_vfs.h>

volatile uint16_t test_passed,test_failed,test_index,test_stage;
volatile uint16_t test_failures[64],test_diagnostics[128];
volatile uint16_t test_unit_code,test_unit_position,test_unit_types;
volatile uint8_t test_done,test_unit_ok,test_unit_error,test_unit_attempted;
Source s;Build b;Diagnostic d;
void test_complete(void);
static void check(uint8_t ok) {
    ++test_index;if(ok)++test_passed;
    else {if(test_failed<64){test_failures[test_failed]=test_index;test_diagnostics[2*test_failed]=d.code;test_diagnostics[2*test_failed+1]=d.start;}++test_failed;}
}
static uint8_t load(const char *text){return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);}
static uint8_t file_text(const char *path,const char *text) {
    int8_t fd;uint16_t n=(uint16_t)strlen(text);uint8_t ok;
    rm(path);fd=open(path,O_WRONLY|O_CREAT|O_TRUNC);if(fd<0)return 0;
    ok=!write(fd,text,&n)&&n==strlen(text);return !close(fd)&&ok;
}
static void closed(void) {
    uint8_t i,count=0;build_close(&b);source_close(&s);
    for(i=0;i<code_required;++i)if(code_pages[i]!=255)++count;
    check(count==15&&code_pages[CODE_RUNTIME]==255&&pages_used()==15);
    for(i=0;i<CODE_BANK_LIMIT;++i)check(code_phase_pending[i]==255);
}
static void author(const char *path,const char *artifact,const char *text,uint8_t error) {
    int8_t fd;check(source_new(&s,path));check(load(text));check(file_text(path,text));
    rm(artifact);fd=open(artifact,O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd>=0){uint8_t ok=compile_unit(&s,&b,&d,fd,0);check((error?!ok&&d.code==error:ok&&!d.code)&&!b.valid);check(!close(fd));}
    closed();
}
static void client(const char *path,const char *text,uint8_t shared) {
    uint8_t status=255;check(source_new(&s,path));check(load(text));check(compile(&s,&b,&d));
    if(b.valid) {
        check(!cc.real_needed);check(!!(far_get(b.pages[0],18)&NATIVE_SHARED_STACK)==shared);
        check(save_build(&s,&b,0));
        {uint8_t ok=run_build(&s,&b);if(ram_result[26]!=1){d.code=ram_result[28];d.start=ram_result[30]|((uint16_t)ram_result[31]<<8);}check(ok&&ram_result[26]==1);}
        check(run_build(&s,&b)&&ram_result[26]==1);
        check(run_saved(&s,&b,&status)&&status==0);
    }
    closed();
}
int app_main(void) {
    int8_t fd;uint8_t status=255;
    if(!console_open())return 1;
    memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=b.sources.page=255;
    test_stage=1;
    author("H:/HWPROBE.PAS","H:/HWPROBE.ZPU",
        "unit HwProbe;interface function Memorize(Value:Word):Word;function ReadState:Word;\n"
        "procedure Poke(var Value:Byte;NewValue:Byte);function Peek(var Value):Byte;\n"
        "function ROMByte:Byte;function ReadMode:Byte;procedure SetMode(Value:Word);\n"
        "procedure Copy(var S,D;Count:Word);implementation var State:Word;ROM:Byte absolute 0;\n"
        "function Memorize(Value:Word):Word;begin Inline($2A/Value/$22/State);Memorize:=State end;\n"
        "function ReadState:Word;begin ReadState:=State end;\n"
        "procedure Poke(var Value:Byte;NewValue:Byte);begin Mem[Word(Addr(Value))]:=NewValue end;\n"
        "function Peek(var Value):Byte;begin Peek:=Mem[Word(Addr(Value))]end;\n"
        "function ROMByte:Byte;begin ROMByte:=ROM end;\n"
        "function ReadMode:Byte;begin ReadMode:=Port[$9C]end;\n"
        "procedure SetMode(Value:Word);begin Port[$9C]:=Value end;\n"
        "procedure Copy(var S,D;Count:Word);begin Move(S,D,Count)end;end.",0);
    check(!rm("H:/HWPROBE.PAS"));
    client("H:/HWPLAIN.PAS","uses HwProbe;var Z:Integer;begin Z:=0;if ReadState<>0 then Z:=1 div Z end.",1);
    client("H:/HWUSE.PAS","uses HwProbe;var Pad:array[0..31]of Word;A,B:array[0..2]of Byte;\n"
        "P:^Byte;W:Word;Z:Integer;Mode:Byte;begin Z:=0;Pad[0]:=1234;\n"
        "W:=Memorize(50000);if(W<>50000)or(ReadState<>50000)or(Pad[0]<>1234)then Z:=1 div Z;\n"
        "Poke(A[0],42);if Peek(A[0])<>42 then Z:=1 div Z;W:=Word(Addr(A[0]));P:=Ptr(W);\n"
        "if Word(P)<>W then Z:=1 div Z;P^:=99;if Peek(A[0])<>99 then Z:=1 div Z;\n"
        "if ROMByte<>Mem[0]then Z:=1 div Z;Mode:=ReadMode;SetMode(Word(Mode));\n"
        "if ReadMode<>Mode then Z:=1 div Z;A[1]:=77;A[2]:=11;W:=3;Copy(A,B,W);\n"
        "if(B[0]<>99)or(B[1]<>77)or(B[2]<>11)then Z:=1 div Z end.",0);
    check(!rm("H:/HWPROBE.ZPU"));check(!exec(EXEC_PRESERVE_PROGRAM,"H:/HWUSE.COM",0,&status)&&status==0);
    author("H:/BADBYTE.PAS","H:/BADBYTE.ZPU","unit BadByte;interface procedure P;implementation var State:Word;procedure P;begin Inline(<State)end;end.",UNIT_UNSUPPORTED);
    author("H:/BADHEAP.PAS","H:/BADHEAP.ZPU","unit BadHeap;interface procedure P;implementation procedure P;var B:^Byte;begin New(B)end;end.",UNIT_UNSUPPORTED);
    author("H:/BADIO.PAS","H:/BADIO.ZPU","unit BadIO;interface procedure P;implementation procedure P;begin Write('x')end;end.",UNIT_UNSUPPORTED);
    /* The focused hardware test omits this optional, separate library probe. */
    test_stage=2;fd=open("H:/ZVB.PAS",O_RDONLY);
    if(fd>=0) {
        test_unit_attempted=1;check(!close(fd));
        check(source_new(&s,"H:/ZVB.PAS"));check(load_source(&s,s.path,&d));
        rm("H:/ZVB.ZPU");fd=open("H:/ZVB.ZPU",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
        if(fd>=0) {
            test_unit_ok=compile_unit(&s,&b,&d,fd,0);
            test_unit_error=d.code;test_unit_position=d.start;
            test_unit_code=cc.emitted>=CC_CODE_BASE?cc.emitted-CC_CODE_BASE:0;test_unit_types=cc.type_count;
            check(!close(fd));
        }
        closed();
    }
    test_done=255;test_complete();return 0;
}
