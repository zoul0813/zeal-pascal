#include "units/internal.h"
#include "platform/banks/loader.h"
#include "program/image.h"
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
        check(!!(far_get(b.pages[0],18)&(NATIVE_SHARED_STACK|NATIVE_SINGLE_PAGE))==!full);
        if(!full)check(b.pages[2]==255);
        check(save_build(&s,&b,0));
        check(run_build(&s,&b)&&ram_result[26]==1);
        check(run_build(&s,&b)&&ram_result[26]==1); /* Per activation, not per image. */
        check(save_build(&s,&b,0));check(run_saved(&s,&b,&status)&&status==0);
    }
    closed();source_close(&s);
}
static const char fault[]="unit InitFault;interface implementation var Z:Integer;begin Z:=0;Z:=1 div Z end.";
static const char fault_client[]="uses InitFault;begin halt end.";
static void fault_location(void) {
    uint8_t file,status=255;uint16_t pc,position;Source view;
    author("H:/INITFAULT.PAS","H:/INITFAULT.ZPU",fault);
    check(!rm("H:/INITFAULT.PAS"));
    check(source_new(&s,"H:/INITERR.PAS"));check(load(fault_client));check(compile(&s,&b,&d));
    if(b.valid) {
        check(build_locate(&s,&b,0,&position)&&position==(uint16_t)(strstr(fault_client,"begin")-fault_client));
        check(save_build(&s,&b,0));check(run_build(&s,&b)&&ram_result[26]==3&&ram_result[28]==2);
        pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
        check(!build_location(&s,&b,pc,&file,&position)&&build_ready(&s,&b));
        check(file_text("H:/INITFAULT.PAS",fault));
        check(build_location(&s,&b,pc,&file,&position)&&file==1);
        check(sources_get(&b.sources,file,&view)&&!strcmp(view.path,"H:/INITFAULT.PAS"));
        check(position==(uint16_t)(strstr(fault,"Z:=1")-fault));
        check(file_text("H:/INITFAULT.PAS","changed"));
        check(!build_location(&s,&b,pc,&file,&position)&&build_ready(&s,&b));
        check(file_text("H:/INITFAULT.PAS",fault));
        check(build_location(&s,&b,pc,&file,&position));
        check(run_saved(&s,&b,&status)&&status==2);
    }
    closed();source_close(&s);check(!rm("H:/INITFAULT.PAS"));check(!rm("H:/INITFAULT.ZPU"));
}
static uint16_t word(uint8_t page,uint16_t at){return far_get(page,at)|((uint16_t)far_get(page,at+1)<<8);}
static void put(uint8_t page,uint16_t at,uint16_t value){far_set(page,at,(uint8_t)value);far_set(page,at+1,(uint8_t)(value>>8));}
static void malformed(void) {
    uint8_t page=page_new(PAGE_COMPILER),bytes[64],i;uint16_t blob,count,at=0,length,n,j;int8_t fd;
    check(page!=255);if(page==255)return;
    fd=open("H:/TRACK.ZPU",O_RDONLY);check(fd>=0);
    if(fd<0){check(page_drop(page));return;}
    do {
        n=sizeof(bytes);check(!read(fd,bytes,&n)&&n<=sizeof(bytes));
        if((uint32_t)at+n>PAGE_SIZE)break;
        for(j=0;j<n;++j)far_set(page,at++,bytes[j]);
    }while(n);
    check(!close(fd));length=at;check(word(page,6)==length);
    blob=word(page,26);count=word(page,blob+6);
    /* Track owns globals, so count names the data fragment plus1. */
    for(i=0;i<2;++i) {
        uint32_t hash=2166136261UL;
        put(page,46,count+i);
        for(at=0;at<length;++at)if(at<28||at>=32)hash=(hash^far_get(page,at))*16777619UL;
        for(j=0;j<4;++j)far_set(page,28+j,(uint8_t)(hash>>(8*j)));
        rm("H:/TRACK.ZPU");fd=open("H:/TRACK.ZPU",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
        if(fd>=0) {
            for(at=0;at<length;at+=n){n=length-at;if(n>64)n=64;check(far_read(page,at,bytes,n));j=n;check(!write(fd,bytes,&j)&&j==n);}
            check(!close(fd));
        }
        check(source_new(&s,"H:/BADINIT.PAS"));check(load("uses Track;begin end."));
        check(!compile(&s,&b,&d)&&d.code==UNIT_FORMAT&&!b.valid);
        build_close(&b);source_close(&s);
    }
    check(page_drop(page));
}
static void chain_activation(void) {
    CompileOptions options={0x9000,0xfbffu,COMPILE_CHAIN};uint8_t status=255;
    author("H:/INITCHAIN.PAS","H:/INITCHAIN.ZPU",
        "unit InitChain;interface function Seen:Integer;implementation uses Crt;var V:Integer;\n"
        "function Seen:Integer;begin Seen:=V end;begin CrtExit;CrtInit;GotoXY(2,3);CrtExit;V:=WhereX*100+WhereY end.");
    check(!rm("H:/INITCHAIN.PAS"));
    check(source_new(&s,"H:/INITCHILD.PAS"));
    check(load("uses InitChain;var Z:Integer;begin Z:=0;if Seen<>709 then Z:=1 div Z;writeln('UNIT-CHAIN-OK')end."));
    check(compile_project(&s,&b,&d,0,&options));
    if(b.valid){check(build_is_chain(&b));check(save_build(&s,&b,0));}
    closed();source_close(&s);
    check(source_new(&s,"H:/INITPARENT.PAS"));
    check(load("uses Crt;var F:File;begin CrtInit;GotoXY(7,9);Assign(F,'H:/INITCHILD.CHN');Chain(F)end."));
    options.kind=COMPILE_PROGRAM;check(compile_project(&s,&b,&d,0,&options));
    if(b.valid){check(save_build(&s,&b,0));check(run_saved(&s,&b,&status)&&status==0);}
    closed();source_close(&s);
}
int app_main(void) {
    uint8_t status=255;int8_t capture;
    if(!console_open())return 1;
    memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=b.sources.page=255;
    test_stage=1;
    author("H:/TRACK.PAS","H:/TRACK.ZPU","unit Track;interface procedure Mark(N:Integer);function Value:Integer;implementation var V:Integer;\n"
        "procedure Mark(N:Integer);begin V:=V*10+N end;function Value:Integer;begin Value:=V end;begin V:=1 end.");
    author("H:/LEFT.PAS","H:/LEFT.ZPU","unit Left;interface uses Track;implementation label 1;var R:record N:Integer end;I:Integer;\n"
        "begin goto 1;Mark(9);1:with R do for I:=1 to 2 do begin N:=I;Mark(2);Exit end;Mark(9)end.");
    author("H:/RIGHT.PAS","H:/RIGHT.ZPU","unit Right;interface implementation uses Track;begin Mark(3)end.");
    author("H:/EMPTY.PAS","H:/EMPTY.ZPU","unit Empty;interface implementation begin ; end.");
    author("H:/INITREAL.PAS","H:/INITREAL.ZPU","unit InitReal;interface uses Track;implementation var R:Real;Z:Integer;\n"
        "begin Mark(5);Z:=0;R:=1.5*2;if R<>3 then Z:=1 div Z end.");
    author("H:/BRIDGE.PAS","H:/BRIDGE.ZPU","unit Bridge;interface uses InitReal;implementation end.");
    author_result("H:/BADINIT.PAS","H:/BADINIT.ZPU","unit BadInit;interface implementation label 1;begin goto 1 end.",40);
    author_result("H:/BADINIT.PAS","H:/BADINIT.ZPU","unit BadInit;interface implementation label 1;procedure P;begin goto 1 end;begin 1: end.",72);
    check(!rm("H:/BADINIT.PAS"));check(!rm("H:/BADINIT.ZPU"));check(!rm("H:/BRIDGE.PAS"));
    check(!rm("H:/TRACK.PAS"));check(!rm("H:/LEFT.PAS"));check(!rm("H:/RIGHT.PAS"));check(!rm("H:/EMPTY.PAS"));check(!rm("H:/INITREAL.PAS"));
    capture=open("H:/UNITINIT.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);if(capture>=0)check(!swap(capture,DEV_STDOUT));
    test_stage=2;
    client("H:/INITUSE.PAS","uses Left,Right,Track;var Z:Integer;begin Z:=0;if Value<>123 then Z:=1 div Z;Mark(4);writeln('UNIT-INIT-OK')end.",1);
    client("H:/REVERSE.PAS","uses Right,Left,Track;var Z:Integer;begin Z:=0;if Value<>132 then Z:=1 div Z;writeln('UNIT-REVERSE-OK')end.",1);
    client("H:/ONLYINIT.PAS","uses InitReal;begin writeln('UNIT-INITREAL-OK')end.",1);
    client("H:/EMPTYUSE.PAS","uses Empty;begin writeln('UNIT-EMPTY-OK')end.",0);
    client("H:/EMPTYRAM.PAS","uses Empty;var N:Integer;begin N:=1 end.",0);
    client("H:/BRIDGERUN.PAS","uses Bridge,Track;var Z:Integer;begin Z:=0;if Value<>15 then Z:=1 div Z;writeln('UNIT-BRIDGE-OK')end.",1);
    chain_activation();
    fault_location();malformed();
    test_stage=3;
    check(!rm("H:/BRIDGE.ZPU"));check(!rm("H:/INITCHAIN.ZPU"));check(!rm("H:/TRACK.ZPU"));check(!rm("H:/LEFT.ZPU"));check(!rm("H:/RIGHT.ZPU"));check(!rm("H:/EMPTY.ZPU"));check(!rm("H:/INITREAL.ZPU"));
    check(!rm("H:/CRT.ZPU"));
    check(!exec(EXEC_PRESERVE_PROGRAM,"H:/INITUSE.COM",0,&status)&&status==0);
    check(!exec(EXEC_PRESERVE_PROGRAM,"H:/REVERSE.COM",0,&status)&&status==0);
    check(!exec(EXEC_PRESERVE_PROGRAM,"H:/ONLYINIT.COM",0,&status)&&status==0);
    check(!exec(EXEC_PRESERVE_PROGRAM,"H:/BRIDGERUN.COM",0,&status)&&status==0);
    check(!exec(EXEC_PRESERVE_PROGRAM,"H:/INITPARENT.COM",0,&status)&&status==0);
    if(capture>=0){check(!swap(capture,DEV_STDOUT));check(!close(capture));}
    check(pages_used()==15);console_close();test_done=255;test_complete();return 0;
}
