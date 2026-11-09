#include "units/internal.h"
#include "compiler/disk.h"
#include "platform/banks/loader.h"
#include "program/image.h"
#include <zos_errors.h>
#include <zos_sys.h>
#include <zos_vfs.h>

volatile uint16_t test_passed,test_failed,test_index,test_stage;
volatile uint16_t test_failures[64],test_diagnostics[128];
volatile uint16_t test_unit_code,test_unit_position,test_unit_types,test_unit_maps,test_unit_relocations;
volatile uint16_t test_clients[16][6],test_client_count;
volatile uint16_t test_storage[5][6],test_storage_count,test_stack_low,test_heap_base;
volatile uint8_t test_fault_pages[32],test_fault_count,test_fault_error;
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
        check(!cc.real_needed);check(!!(far_get(b.pages[0],18)&(NATIVE_SHARED_STACK|NATIVE_SINGLE_PAGE))==shared);
        check(save_build(&s,&b,0));
        {uint8_t ok=run_build(&s,&b);if(ram_result[26]!=1){d.code=ram_result[28];d.start=ram_result[30]|((uint16_t)ram_result[31]<<8);}check(ok&&ram_result[26]==1);}
        check(run_build(&s,&b)&&ram_result[26]==1);
        check(run_saved(&s,&b,&status)&&status==0);
    }
    closed();
}
static void file_client(const char *path,uint8_t demo,uint8_t real) {
    uint16_t index;uint8_t status=255,ok;int8_t fd;char executable[128],owned_tail[64];const char *arguments[1];
    fd=open(path,O_RDONLY);if(fd<0)return;check(!close(fd));index=test_client_count++;
    test_stage=10+index;
    check(source_new(&s,path));check(load_source(&s,path,&d));
    /* Snake exercises the existing disk-backed Program compilation path. */
    compiler_disk_requested=demo==3;
    ok=compile(&s,&b,&d);compiler_disk_requested=0;check(ok&&b.valid);
    test_clients[index][4]=d.code;test_clients[index][5]=d.start;
    if(!ok){test_clients[index][1]=cc.emitted>=CC_CODE_BASE?cc.emitted-CC_CODE_BASE:0;test_clients[index][2]=b.globals_bytes;}
    if(b.valid) {
        if(demo>=3)check(b.valid==(demo==3?2:1));
        test_clients[index][0]=image_get(&b,18);
        test_clients[index][1]=b.code_bytes;test_clients[index][2]=b.globals_bytes;
        test_clients[index][3]=b.length;
        check(!!cc.real_needed==real);check(save_build(&s,&b,0));
        strcpy(executable,path);strcpy(executable+strlen(executable)-3,"COM");
        if(!demo) {
            ok=run_build(&s,&b);
            if(ram_result[26]!=1){d.code=ram_result[28];d.start=ram_result[30]|((uint16_t)ram_result[31]<<8);}
            check(ok&&ram_result[26]==1);
        }
        build_close(&b);source_close(&s);
        if(demo==2||demo>=3) {
            fd=open(demo==2?"H:/CRCOUT.TXT":"H:/SNAKEOUT.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
            if(fd>=0) {
                ok=!swap(fd,DEV_STDOUT);check(ok);
                if(ok) {
                    strcpy(owned_tail,demo==2?"H:/EMPTY.DAT H:/DIGITS.DAT H:/LARGE.DAT":"--demo");
                    arguments[0]=owned_tail;
                    ok=exec(EXEC_PRESERVE_PROGRAM,executable,arguments,&status);
                    d.start=ok;ok=!ok;
                    check(!swap(fd,DEV_STDOUT));
                    d.code=status;check(ok&&status==0);
                }
                check(!close(fd));
            }
        } else {
            strcpy(owned_tail,"--demo");arguments[0]=owned_tail;
            ok=!exec(EXEC_PRESERVE_PROGRAM,executable,demo?arguments:0,&status);
            d.code=status;check(ok&&status==0);
        }
    }
    closed();
}

static uint16_t heap_base(void) __naked {
    __asm
        .globl s__HEAP
        ld de,#s__HEAP
        ret
    __endasm;
}
static uint8_t present(const char *path) {
    int8_t fd=open(path,O_RDONLY);if(fd<0)return 0;check(!close(fd));return 1;
}
/* The fault fixture calls this only through an authenticated temporary patch
 * of page_new, after metadata release. Public OS allocations hold every spare
 * page until failed serialization has restored the compiler cache. */
void test_exhaust_pages(void) __banked {
    uint8_t page,error;
    while(test_fault_count<32) {
        error=palloc(&page);if(error){test_fault_error=error;return;}
        test_fault_pages[test_fault_count++]=page;
    }
}
static void storage_author(const char *path,const char *artifact,uint8_t error) {
    int8_t fd;uint8_t ok;uint16_t index;
    if(!present(path))return;
    index=test_storage_count++;test_stage=40+index;
    check(source_new(&s,path));check(load_source(&s,path,&d));
    rm(artifact);fd=open(artifact,O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd>=0) {
        ok=compile_unit(&s,&b,&d,fd,1);
        test_storage[index][0]=cc.emitted-CC_CODE_BASE;
        test_storage[index][1]=cc.entries;test_storage[index][2]=cc.relocations;
        test_storage[index][3]=d.code;test_storage[index][4]=d.start;test_storage[index][5]=platform_error;
        check(error?!ok&&d.code==error:ok&&!d.code);check(!close(fd));
        if(test_fault_error)check(test_storage[index][5]==test_fault_error);
        while(test_fault_count)check(!pfree(test_fault_pages[--test_fault_count]));
        test_fault_error=0;
    }
    closed();
}
static void reject_file(const char *path,uint8_t error) {
    if(!present(path))return;
    check(source_new(&s,path));check(load_source(&s,path,&d));
    check(!compile(&s,&b,&d)&&d.code==error&&!b.valid);closed();
}
void storage_checkpoint(void);
static void storage_suite(void) {
    uint8_t error=present("H:/CANCEL.RUN")?COMPILE_INTERRUPTED:
        present("H:/ALLOC.RUN")?99:0;
    if(error) {
        storage_author("H:/PAGED.PAS","H:/PAGED.ZPU",error);
        storage_author("H:/BASE.PAS","H:/BASE.ZPU",0);return;
    }
    storage_author("H:/BASE.PAS","H:/BASE.ZPU",0);
    storage_author("H:/PAGED.PAS","H:/PAGED.ZPU",0);
    storage_author("H:/HIGH.PAS","H:/HIGH.ZPU",0);
    test_stage=49;storage_checkpoint();
    storage_author("H:/LAST.PAS","H:/LAST.ZPU",0);
    storage_author("H:/OVERCODE.PAS","H:/OVERCODE.ZPU",99);
    file_client("H:/PGCLIENT.PAS",0,1);file_client("H:/HICLIENT.PAS",0,0);
    file_client("H:/LACLIENT.PAS",0,0);file_client("H:/BOUND.PAS",0,0);
    reject_file("H:/FAILBIG.PAS",99);reject_file("H:/OVERFILE.PAS",UNIT_FORMAT);
    test_stage=60;storage_checkpoint();
    if(present("H:/PGCLIENT.COM")) {
        uint8_t status=255;
        check(!exec(EXEC_PRESERVE_PROGRAM,"H:/PGCLIENT.COM",0,&status)&&status==0);
    }
    if(present("H:/HICLIENT.COM")) {
        uint8_t status=255;
        check(!exec(EXEC_PRESERVE_PROGRAM,"H:/HICLIENT.COM",0,&status)&&status==0);
    }
}

int app_main(void) {
    int8_t fd;uint8_t status=255;
    if(!console_open())return 1;
    memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=b.sources.page=255;
    test_heap_base=heap_base();check(test_heap_base<0xf000u);
    if(test_heap_base<0xf000u)memset((void*)test_heap_base,0xa5,0xf000u-test_heap_base);
    if(present("H:/STORAGE.RUN")){storage_suite();goto done;}
    fd=open("H:/SKIPHW.RUN",O_RDONLY);
    if(fd>=0){check(!close(fd));goto unit_probe;}
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
        "if(B[0]<>99)or(B[1]<>77)or(B[2]<>11)then Z:=1 div Z;\n"
        "W:=32768;Copy(Mem[$4000],Mem[$4000],W);W:=0;Copy(A,B,W);FillChar(B,W,123);\n"
        "if(B[0]<>99)or(B[1]<>77)or(B[2]<>11)then Z:=1 div Z end.",0);
    check(!rm("H:/HWPROBE.ZPU"));check(!exec(EXEC_PRESERVE_PROGRAM,"H:/HWUSE.COM",0,&status)&&status==0);
    author("H:/BADBYTE.PAS","H:/BADBYTE.ZPU","unit BadByte;interface procedure P;implementation var State:Word;procedure P;begin Inline(<State)end;end.",UNIT_UNSUPPORTED);
    author("H:/MIXFIX.PAS","H:/MIXFIX.ZPU","unit MixFix;interface procedure P;implementation var State:Word;procedure P;begin Inline(State+*)end;end.",UNIT_UNSUPPORTED);
    author("H:/TWODATA.PAS","H:/TWODATA.ZPU","unit TwoData;interface procedure P;implementation var State:Word;procedure P;begin Inline(State+State)end;end.",UNIT_UNSUPPORTED);
    author("H:/NEGDATA.PAS","H:/NEGDATA.ZPU","unit NegData;interface procedure P;implementation var State:Word;procedure P;begin Inline(5-State)end;end.",UNIT_UNSUPPORTED);
    author("H:/BYTECODE.PAS","H:/BYTECODE.ZPU","unit ByteCode;interface procedure P;implementation procedure P;begin Inline(<P)end;end.",UNIT_UNSUPPORTED);
    author("H:/LOWDATA.PAS","H:/LOWDATA.ZPU","unit LowData;interface procedure P;implementation var State:Word;procedure P;begin Inline(State-1)end;end.",UNIT_FORMAT);
    author("H:/ENDDATA.PAS","H:/ENDDATA.ZPU","unit EndData;interface procedure P;implementation var State:Word;procedure P;begin Inline(State+2)end;end.",UNIT_FORMAT);
    author("H:/LASTDATA.PAS","H:/LASTDATA.ZPU","unit LastData;interface procedure P;implementation var State:Word;procedure P;begin Inline(State+1)end;end.",0);
    author("H:/BADHEAP.PAS","H:/BADHEAP.ZPU","unit BadHeap;interface procedure P;implementation procedure P;var B:^Byte;begin New(B)end;end.",UNIT_UNSUPPORTED);
    author("H:/BADIO.PAS","H:/BADIO.ZPU","unit BadIO;interface procedure P;implementation procedure P;begin Write('x')end;end.",UNIT_UNSUPPORTED);
    /* The focused hardware test omits this optional, separate library probe. */
unit_probe:
    test_stage=2;fd=open("H:/ZVB.PAS",O_RDONLY);
    if(fd>=0) {
        test_unit_attempted=1;check(!close(fd));
        check(source_new(&s,"H:/ZVB.PAS"));check(load_source(&s,s.path,&d));
        rm("H:/ZVB.ZPU");fd=open("H:/ZVB.ZPU",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
        if(fd>=0) {
            test_unit_ok=compile_unit(&s,&b,&d,fd,0);
            test_unit_error=d.code;test_unit_position=d.start;
            test_unit_code=cc.emitted>=CC_CODE_BASE?cc.emitted-CC_CODE_BASE:0;test_unit_types=cc.type_count;test_unit_maps=cc.entries;test_unit_relocations=cc.relocations;
            check(!close(fd));
        }
        closed();
    }
        if(test_unit_ok||present("H:/PREBUILT.RUN")) {
            fd=open("H:/CLIENTS.RUN",O_RDONLY);
            if(fd>=0) {
                check(!close(fd));
                file_client("H:/USES.PAS",0,0);file_client("H:/ORDER.PAS",0,0);
                file_client("H:/SMOKE.PAS",0,0);file_client("H:/CRC.PAS",0,0);
                file_client("H:/DMA.PAS",0,0);file_client("H:/GFX.PAS",0,0);
                file_client("H:/PACK.PAS",0,0);file_client("H:/OPTIONS.PAS",0,0);
                file_client("H:/PAGE.PAS",0,0);file_client("H:/SOUND.PAS",0,0);
                file_client("H:/ZVBCHECK.PAS",0,0);
                file_client("H:/SPRITES.PAS",1,0);file_client("H:/SNAKE.PAS",present("H:/DISKSNAKE.RUN")?3:4,0);
                file_client("H:/FIRSTBAR.PAS",1,0);file_client("H:/CRC32.PAS",2,0);
            }
        }
done:
    test_stack_low=test_heap_base;
    while(test_stack_low<0xf000u&&*(uint8_t*)test_stack_low==0xa5)++test_stack_low;
    check(test_stack_low-test_heap_base>=2048u);
    test_done=255;test_complete();return 0;
}
