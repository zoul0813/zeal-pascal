#include "compiler_units.h"
#include "compiler_job.h"
#include "library.h"
#include "output_transport.h"
#include <zos_sys.h>
#include <zos_vfs.h>

volatile uint16_t test_passed,test_failed,test_index,test_stage,test_failures[64];
volatile uint8_t test_done;
Source s;Build b;Diagnostic d;
void test_complete(void);
static void check(uint8_t ok) {
    ++test_index;
    if(ok)++test_passed;
    else {if(test_failed<64)test_failures[test_failed]=test_index;++test_failed;}
}
static uint8_t load(const char *text) {
    return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);
}
static void closed(void) {
    uint8_t i,count=0;build_close(&b);
    for(i=0;i<code_required;++i)if(code_pages[i]!=255)++count;
    check(count==15&&code_pages[CODE_RUNTIME]==255&&pages_used()==16);
    for(i=0;i<CODE_BANK_LIMIT;++i)check(code_phase_pending[i]==255);
}
static const char mixed[]=
    "unit Mixed;interface\n"
    "function Plain(X:Integer):Integer;\nfunction Scale(X:Real):Real;\n"
    "function CopyReal(X:Real):Real;\nfunction Fault(X:Real):Real;\nfunction Transitive(X:Integer):Integer;\n"
    "implementation\nfunction Hidden(X:Real):Real;begin Hidden:=X*2 end;\n"
    "function Plain(X:Integer):Integer;begin Plain:=X+1 end;\n"
    "function Scale(X:Real):Real;begin Scale:=Hidden(X) end;\n"
    "function CopyReal(X:Real):Real;begin CopyReal:=X end;\n"
    "function Fault(X:Real):Real;begin Fault:=1.0/X end;\n"
    "function Transitive(X:Integer):Integer;var R:Real;\n"
    "begin R:=Hidden(X);Transitive:=Trunc(R)end;end.";
static uint8_t author(const char *path,const char *artifact,const char *text) {
    int8_t fd;uint8_t ok;uint16_t n=(uint16_t)strlen(text);
    check(source_new(&s,path));check(load(text));
    fd=open(path,O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)return 0;
    check(!write(fd,text,&n)&&n==strlen(text));check(!close(fd));
    fd=open(artifact,O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)return 0;
    ok=compile_unit(&s,&b,&d,fd,0);check(ok&&!d.code&&!b.valid);
    check(!close(fd));closed();source_close(&s);return ok;
}
static void client(const char *path,const char *text) {
    uint8_t status=255;
    check(source_new(&s,path));check(load(text));check(compile(&s,&b,&d));
    if(b.valid) {
        check(save_build(&s,&b,0));check(run_build(&s,&b)&&ram_result[26]==1);
        check(run_saved(&s,&b,&status)&&status==0);
    }
    closed();source_close(&s);
}
static void fault_location(void) {
    uint16_t pc,position;uint8_t file,status=255;Source view;
    check(source_new(&s,"H:/FAULTRUN.PAS"));
    check(load("uses Mixed;var R:Real;begin R:=Fault(0.0)end."));check(compile(&s,&b,&d));
    if(b.valid) {
        check(save_build(&s,&b,0));check(run_build(&s,&b)&&ram_result[26]==3&&ram_result[28]==2);
        pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
        check(build_location(&s,&b,pc,&file,&position)&&file==1);
        check(sources_get(&b.sources,file,&view)&&!strcmp(view.path,"H:/MIXED.PAS"));
        check(position==(uint16_t)(strstr(mixed,"Fault:=")-mixed));
        check(run_saved(&s,&b,&status)&&status==2);
    }
    closed();source_close(&s);
}
static uint16_t page_word(uint8_t page,uint16_t at) {
    return far_get(page,at)|((uint16_t)far_get(page,at+1)<<8);
}
static void page_word_set(uint8_t page,uint16_t at,uint16_t value) {
    far_set(page,at,(uint8_t)value);far_set(page,at+1,(uint8_t)(value>>8));
}
static void checksum(uint8_t page,uint16_t bytes) {
    uint16_t at;uint8_t i;uint32_t value=2166136261UL;
    for(at=0;at<bytes;++at)if(at<28||at>=32)value=(value^far_get(page,at))*16777619UL;
    for(i=0;i<4;++i)far_set(page,28+i,(uint8_t)(value>>(8*i)));
}
static uint16_t read_artifact(const char *path,uint8_t page) {
    uint8_t bytes[64];uint16_t at=0,n,i;int8_t fd=open(path,O_RDONLY);
    check(fd>=0);if(fd<0)return 0;
    do {
        n=sizeof(bytes);check(!read(fd,bytes,&n)&&n<=sizeof(bytes));
        if((uint32_t)at+n>PAGE_SIZE)break;
        for(i=0;i<n;++i)far_set(page,at++,bytes[i]);
    }while(n);
    check(!close(fd));return at;
}
static void write_artifact(const char *path,uint8_t page,uint16_t bytes) {
    uint8_t pages[3]={255,255,255};char owned_path[128];strcpy(owned_path,path);pages[0]=page;
    check(!rm(owned_path));check(output_store_range(pages,0,bytes,owned_path));
}
static void artifact_negatives(void) {
    uint8_t page=page_new(PAGE_COMPILER),saved;uint16_t bytes,blob,fragments;
    check(page!=255);if(page==255)return;
    bytes=read_artifact("H:/MIXED.ZPU",page);blob=page_word(page,26);fragments=page_word(page,blob+6);
    saved=far_get(page,blob-fragments);far_set(page,blob-fragments,2);checksum(page,bytes);
    write_artifact("H:/MIXED.ZPU",page,bytes);
    check(source_new(&s,"H:/BADFLAGS.PAS"));check(load("uses Mixed;begin end."));
    check(!compile(&s,&b,&d)&&d.code==UNIT_FORMAT&&!b.valid);
    build_close(&b);source_close(&s);check(pages_used()==16);
    far_set(page,blob-fragments,saved);checksum(page,bytes);write_artifact("H:/MIXED.ZPU",page,bytes);
    check(page_drop(page));check(pages_used()==15);
}
static void legacy_artifact(void) {
    uint8_t page=page_new(PAGE_COMPILER);uint16_t bytes,blob,fragments,at;
    check(page!=255);if(page==255)return;
    bytes=read_artifact("H:/LEGACY.ZPU",page);blob=page_word(page,26);fragments=page_word(page,blob+6);
    for(at=blob;at<bytes;++at)far_set(page,at-fragments,far_get(page,at));
    bytes-=fragments;page_word_set(page,4,ZPU_LEGACY_ABI);page_word_set(page,6,bytes);
    page_word_set(page,26,blob-fragments);checksum(page,bytes);
    write_artifact("H:/LEGACY.ZPU",page,bytes);check(page_drop(page));check(pages_used()==15);
}
static void io_pressure(void) {
    uint8_t pages[32],count=0;Source before;
    check(source_new(&s,"H:/PRESSURE.PAS"));before=s;
    while(count<32&&!palloc(pages+count))++count;
    check(count<32);check(!code_phase_unit_io()&&platform_error);
    check(code_pages[CODE_UNIT_IO]==255&&code_phase_pending[CODE_UNIT_IO]==255);
    check(!memcmp(&s,&before,sizeof(s)));
    while(count)check(!pfree(pages[--count]));closed();source_close(&s);
}
static void missing_main(void) {
    Source root;CompileFailure failure;
    memset(&root,0,sizeof(root));root.page=255;strcpy(root.path,"H:/MISSING.PAS");
    check(!compile_request(&root,&b,&failure,COMPILER_UNIT_MODE,0));
    check(failure.diagnostic.code==90&&!strcmp(failure.path,root.path));
    check(!b.valid&&pages_used()==15);
    check(!memcmp(compiler_owned,"\0\0\0\0\0\0\0\0",8));
}
int app_main(void) {
    uint8_t status;int8_t capture;
    if(!console_open())return 1;
    memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=b.sources.page=255;
    test_stage=1;io_pressure();missing_main();
    if(!author("H:/MIXED.PAS","H:/MIXED.ZPU",mixed))goto done;
    artifact_negatives();
    if(!author("H:/LEGACY.PAS","H:/LEGACY.ZPU",
        "unit Legacy;interface function Convert(X:Integer):Integer;implementation\n"
        "function Convert(X:Integer):Integer;var R:Real;\n"
        "begin R:=X*2.0;Convert:=Trunc(R)end;end."))goto done;
    legacy_artifact();
    capture=open("H:/UNITREAL.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(!swap(capture,DEV_STDOUT));
    test_stage=2;
    client("H:/PLAIN.PAS","uses Mixed;begin if Plain(41)<>42 then halt;writeln('UNIT-INTEGER-OK')end.");
    client("H:/COPY.PAS","uses Mixed;var A,R:Real;P,Q,I:Integer;begin\n"
        "A:=1.5;R:=CopyReal(A);P:=Addr(A);Q:=Addr(R);\n"
        "for I:=0 to 5 do if Mem[P+I]<>Mem[Q+I]then halt;writeln('UNIT-COPY-OK')end.");
    client("H:/SCALE.PAS","uses Mixed;var A,R:Real;P,Q,I:Integer;begin\n"
        "A:=3.0;R:=Scale(1.5);P:=Addr(A);Q:=Addr(R);\n"
        "for I:=0 to 5 do if Mem[P+I]<>Mem[Q+I]then halt;writeln('UNIT-SCALE-OK')end.");
    client("H:/TRANSIT.PAS","uses Mixed;begin if Transitive(3)<>6 then halt;writeln('UNIT-TRANSITIVE-OK')end.");
    client("H:/UNUSED.PAS","uses Mixed;begin writeln('UNIT-UNUSED-OK')end.");
    client("H:/OLDCLIENT.PAS","uses Legacy;begin if Convert(3)<>6 then halt;writeln('UNIT-LEGACY-OK')end.");
    fault_location();
    test_stage=3;check(!rm("H:/LEGACY.ZPU"));check(!rm("H:/LEGACY.PAS"));check(!rm("H:/MIXED.ZPU"));check(!rm("H:/MIXED.PAS"));
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/SCALE.COM",0,&status)&&status==0);
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/TRANSIT.COM",0,&status)&&status==0);
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/OLDCLIENT.COM",0,&status)&&status==0);
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/FAULTRUN.COM",0,&status)&&status==2);
    if(capture>=0){check(!swap(capture,DEV_STDOUT));check(!close(capture));}
done:
    check(pages_used()==15);console_close();test_done=255;test_complete();return 0;
}
