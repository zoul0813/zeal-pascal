#include "compiler_units.h"
#include "compiler_job.h"
#include "library.h"
#include "image.h"
#include <zos_errors.h>
#include "output_transport.h"
#include <zos_sys.h>
#include <zos_vfs.h>

volatile uint16_t test_passed,test_failed,test_index,test_stage,test_failures[64],test_diagnostics[128];
volatile uint8_t test_done;
Source s;Build b;Diagnostic d;
void test_complete(void);
static void check(uint8_t ok) {
    ++test_index;
    if(ok)++test_passed;
    else {if(test_failed<64){test_failures[test_failed]=test_index;test_diagnostics[2*test_failed]=d.code;test_diagnostics[2*test_failed+1]=d.start;}++test_failed;}
}
static uint8_t load(const char *text) {
    return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);
}
static void closed_extra(uint8_t extra) {
    uint8_t i,count=0;build_close(&b);
    for(i=0;i<code_required;++i)if(code_pages[i]!=255)++count;
    check(count==15&&code_pages[CODE_RUNTIME]==255&&pages_used()==16+extra);
    for(i=0;i<CODE_BANK_LIMIT;++i)check(code_phase_pending[i]==255);
}
static void closed(void){closed_extra(0);}
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
    rm(path);fd=open(path,O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)return 0;
    check(!write(fd,text,&n)&&n==strlen(text));check(!close(fd));
    rm(artifact);fd=open(artifact,O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)return 0;
    ok=compile_unit(&s,&b,&d,fd,0);check(ok&&!d.code&&!b.valid);
    check(!close(fd));closed();source_close(&s);return ok;
}
static uint8_t source_file(const char *path,const char *text) {
    int8_t fd;
    uint16_t bytes=(uint16_t)strlen(text);
    uint8_t ok;
    rm(path);fd=open(path,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0)return 0;
    ok=!write(fd,text,&bytes)&&bytes==strlen(text);
    return !close(fd)&&ok;
}
static void client(const char *path,const char *text) {
    uint8_t status=255;
    check(source_new(&s,path));check(load(text));check(compile(&s,&b,&d));
    if(b.valid) {
        check(save_build(&s,&b,0));check(run_build(&s,&b)&&ram_result[26]==1);
        check(save_build(&s,&b,0)); /* save the post-Run image, too */
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
        check(!build_location(&s,&b,pc,&file,&position)&&build_ready(&s,&b));
        check(source_file("H:/MIXED.PAS",mixed));
        check(build_location(&s,&b,pc,&file,&position)&&file==1);
        check(sources_get(&b.sources,file,&view)&&!strcmp(view.path,"H:/MIXED.PAS"));
        check(position==(uint16_t)(strstr(mixed,"Fault:=")-mixed));
        check(source_file("H:/MIXED.PAS","changed source"));
        check(!build_location(&s,&b,pc,&file,&position)&&build_ready(&s,&b));
        check(source_file("H:/MIXED.PAS",mixed));
        check(build_location(&s,&b,pc,&file,&position)&&file==1);
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
    uint8_t page=page_new(PAGE_COMPILER),saved;uint16_t bytes,blob,fragments,source_length;
    check(page!=255);if(page==255)return;
    bytes=read_artifact("H:/MIXED.ZPU",page);blob=page_word(page,26);fragments=page_word(page,blob+6);
    page_word_set(page,4,0xffffu);checksum(page,bytes);write_artifact("H:/MIXED.ZPU",page,bytes);
    check(source_new(&s,"H:/BADVERSION.PAS"));check(load("uses Mixed;begin end."));
    check(!compile(&s,&b,&d)&&d.code==UNIT_FORMAT&&!b.valid);
    build_close(&b);source_close(&s);check(pages_used()==16);
    page_word_set(page,4,ZPU_ABI);
    saved=far_get(page,blob-fragments);far_set(page,blob-fragments,8);checksum(page,bytes);
    write_artifact("H:/MIXED.ZPU",page,bytes);
    check(source_new(&s,"H:/BADFLAGS.PAS"));check(load("uses Mixed;begin end."));
    check(!compile(&s,&b,&d)&&d.code==UNIT_FORMAT&&!b.valid);
    build_close(&b);source_close(&s);check(pages_used()==16);
    far_set(page,blob-fragments,saved);source_length=page_word(page,16);
    page_word_set(page,16,65535u);checksum(page,bytes);write_artifact("H:/MIXED.ZPU",page,bytes);
    check(source_new(&s,"H:/BADLENGTH.PAS"));check(load("uses Mixed;begin end."));
    check(!compile(&s,&b,&d)&&d.code==UNIT_FORMAT&&!b.valid);
    build_close(&b);source_close(&s);check(pages_used()==16);
    page_word_set(page,16,1);checksum(page,bytes);write_artifact("H:/MIXED.ZPU",page,bytes);
    check(source_new(&s,"H:/BADMAP.PAS"));check(load("uses Mixed;begin end."));
    check(!compile(&s,&b,&d)&&d.code==UNIT_FORMAT&&!b.valid);
    build_close(&b);source_close(&s);check(pages_used()==16);
    page_word_set(page,16,source_length);saved=far_get(page,8);far_set(page,8,saved^1);
    checksum(page,bytes);write_artifact("H:/MIXED.ZPU",page,bytes);
    check(source_new(&s,"H:/BADPROFILE.PAS"));check(load("uses Mixed;begin end."));
    check(!compile(&s,&b,&d)&&d.code==UNIT_STALE&&!b.valid);
    build_close(&b);source_close(&s);check(pages_used()==16);
    far_set(page,8,saved);checksum(page,bytes);write_artifact("H:/MIXED.ZPU",page,bytes);
    check(page_drop(page));check(pages_used()==15);
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
/* Exhaust only pages allocated through the public parent ledger. The
 * compiler banks are parked first, exactly as execution will park them. */
static void page_pressure(uint8_t shared) {
    uint8_t held[32],count=0,page;
    check(source_new(&s,"H:/PRESSURE.PAS"));
    check(load(shared?"begin end.":"var P:^Integer;begin New(P);P^:=42;Dispose(P)end."));
    check(compile(&s,&b,&d));
    if(b.valid) {
        check(!!(far_get(b.pages[0],18)&NATIVE_SHARED_STACK)==shared);
        check(b.pages[2]==255);
        check(code_phase_park());
        while(count<32) {
            page=page_new(PAGE_RUN);
            if(page==255)break;
            held[count++]=page;
        }
        check(count>0&&platform_error==ERR_NO_MORE_MEMORY);
        if(shared) {
            check(run_build(&s,&b)&&ram_result[26]==1);
            check(ram_pages[1]==ram_pages[2]&&b.pages[2]==255);
        } else {
            check(!run_build(&s,&b)&&platform_error==ERR_NO_MORE_MEMORY);
            check(b.pages[2]==255&&build_ready(&s,&b));
            if(!count)goto restore;
            check(page_drop(held[--count]));
            check(run_build(&s,&b)&&ram_result[26]==1);
            check(ram_pages[1]!=ram_pages[2]);
        }
        while(count)check(page_drop(held[--count]));
restore:
        check(code_phase_restore());
    }
    closed();source_close(&s);
}
/* Eligibility is intentionally conservative for helpers whose stack demand is
 * not part of the bounded arithmetic contract. */
static void layout_helpers(void) {
    static const char *const cases[]={
        "begin WriteLn end.","var N:Integer;begin ReadLn(N)end.",
        "var N,C:Integer;begin Val('42',N,C)end.",
        "var S:String[10];begin Str(42,S)end.",
        "var N:Integer;begin N:=Random(7)end.",
        "var B:Boolean;begin B:=KeyPressed end.","uses Crt;begin ClrScr end.",
        "{$U+}begin end.","begin Inline($00)end."
    };
    uint8_t i,header[160];NativeImage id;
    for(i=0;i<sizeof(cases)/sizeof(*cases);++i) {
        check(source_new(&s,"H:/HELPER.PAS"));check(load(cases[i]));
        check(compile(&s,&b,&d));
        if(b.valid) {
            check(!(far_get(b.pages[0],18)&NATIVE_SHARED_STACK));
            check(far_read(b.pages[0],0,header,sizeof(header))&&!native_header(header,0,&id));
            if(i!=1)check(run_build(&s,&b)&&ram_result[26]==1);
        }
        closed();source_close(&s);
    }
}
static void layout_limits(void) {
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};CompileFailure failure;
    uint16_t bytes,limit,value;uint8_t i,header[160];NativeImage id;char text[96],*digit;
    check(source_new(&s,"H:/LIMIT.PAS"));check(load("begin end."));
    check(compile(&s,&b,&d));bytes=b.code_bytes;
    check(far_get(b.pages[0],18)&NATIVE_SHARED_STACK);
    closed();source_close(&s);
    /* The emitted stub is70 bytes; the selector reserves128. */
    limit=0xb400u-bytes-(128u-70u);
    for(i=0;i<4;++i) {
        check(source_new(&s,"H:/LIMIT.PAS"));
        if(i<2) {
            options.start=limit+i;check(load("begin end."));
        } else {
            options.start=0;
            strcpy(text,"var A:array[1..00000]of Byte;begin end.");
            digit=strstr(text,"00000")+5;value=limit-0x8000u+(i-2);
            do {*--digit='0'+value%10;value/=10;}while(value);
            check(load(text));
        }
        check(compile_request(&s,&b,&failure,0,&options));
        if(b.valid) {
            check(!!(far_get(b.pages[0],18)&NATIVE_SHARED_STACK)==!(i&1));
            check(far_read(b.pages[0],0,header,sizeof(header))&&!native_header(header,0,&id));
            check(run_build(&s,&b)&&ram_result[26]==1);
        }
        closed();source_close(&s);
    }
}
static void execute_shared(void) {
    uint8_t status=255;
    check(source_new(&s,"H:/EXECALIAS.PAS"));
    check(load("var F:File;begin Assign(F,'H:/SHARED.COM');Execute(F)end."));
    check(compile(&s,&b,&d));
    if(b.valid) {
        check(!(far_get(b.pages[0],18)&NATIVE_SHARED_STACK));
        check(save_build(&s,&b,0));
        /* Memory mode must refuse transfer before retiring any Build owner. */
        check(run_build(&s,&b)&&ram_result[26]==3&&ram_result[28]==21&&build_ready(&s,&b));
        check(run_saved(&s,&b,&status)&&status==0);
    }
    closed();source_close(&s);
}
/* Formal VAR arguments occupy two bytes, Real values six. The final case
 * also proves that the live count is restored after nested and completed calls. */
static void argument_calls(void) {
    static const char *const paths[]={"H:/ARG512.PAS","H:/ARG516.PAS","H:/ARGPEAK.PAS","H:/ARGSMALL.PAS"};
    uint8_t i,shared,status,header[160];int8_t fd;NativeImage id;
    check(read_block(&s,"H:/WIDE.PAS",&d));
    fd=open("H:/WIDE.ZPU",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd>=0) {
        check(compile_unit(&s,&b,&d,fd,0)&&!d.code);check(!close(fd));
    }
    closed();source_close(&s);
    for(i=0;i<4;++i) {
        shared=i==0||i==3;
        check(read_block(&s,paths[i],&d));check(compile(&s,&b,&d));
        if(b.valid) {
            check(!!(far_get(b.pages[0],18)&NATIVE_SHARED_STACK)==shared);
            check(far_read(b.pages[0],0,header,sizeof(header))&&!native_header(header,0,&id));
            check(run_build(&s,&b)&&ram_result[26]==1);
            check((ram_pages[1]==ram_pages[2])==shared);
            check(save_build(&s,&b,0));status=255;
            check(run_saved(&s,&b,&status)&&status==0);
        }
        closed();source_close(&s);
    }
    check(!rm("H:/WIDE.ZPU"));check(!rm("H:/WIDE.PAS"));
}
static const char shared_unit[]=
    "unit Shared;interface procedure Reset;function Next:Integer;\n"
    "implementation var Count:Integer;procedure Reset;begin Count:=0 end;\n"
    "function Next:Integer;begin Count:=Count+1;Next:=Count end;end.";
static void dependency_units(void) {
    check(author("H:/SHARED.PAS","H:/SHARED.ZPU",shared_unit));
    check(author("H:/LEFTDEP.PAS","H:/LEFTDEP.ZPU",
        "unit LeftDep;interface uses Shared;function Step:Integer;\n"
        "implementation function Step:Integer;begin Step:=Shared.Next end;end."));
    check(author("H:/RIGHTDEP.PAS","H:/RIGHTDEP.ZPU",
        "unit RightDep;interface function Other:Integer;\n"
        "implementation uses Shared;function Other:Integer;begin Other:=Next end;end."));
    check(!rm("H:/SHARED.PAS"));check(!rm("H:/LEFTDEP.PAS"));check(!rm("H:/RIGHTDEP.PAS"));
    client("H:/DIAMOND.PAS","uses LeftDep,RightDep,Shared;var N:Integer;begin Reset;\n"
        "N:=Step;if N<>1 then N:=1 div(N-N);N:=Other;if N<>2 then N:=1 div(N-N);\n"
        "writeln('UNIT-DEPS-OK')end.");
    check(source_new(&s,"H:/HIDDEN.PAS"));check(load("uses LeftDep;begin Shared.Reset end."));
    check(!compile(&s,&b,&d)&&d.code==41);closed();source_close(&s);
    check(!rm("H:/SHARED.ZPU"));
    check(source_new(&s,"H:/MISSING.PAS"));check(load("uses LeftDep;begin end."));
    check(!compile(&s,&b,&d)&&d.code==UNIT_MISSING);closed();source_close(&s);
    check(author("H:/SHARED.PAS","H:/SHARED.ZPU",shared_unit));
    /* Replacing the dependency with any different artifact invalidates its users. */
    check(author("H:/SHARED.PAS","H:/SHARED.ZPU",
        "unit Shared;interface procedure Reset;function Next:Integer;\n"
        "implementation procedure Reset;begin end;function Next:Integer;begin Next:=9 end;end."));
    check(source_new(&s,"H:/STALE.PAS"));check(load("uses LeftDep;begin end."));
    check(!compile(&s,&b,&d)&&d.code==UNIT_STALE);closed();source_close(&s);
    /* The current authoring unit cannot depend on itself through another unit. */
    check(source_new(&s,"H:/SHARED.PAS"));
    check(load("unit Shared;interface uses LeftDep;implementation end."));
    {
        int8_t fd=open("H:/CYCLE.ZPU",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
        if(fd>=0){check(!compile_unit(&s,&b,&d,fd,0)&&d.code==UNIT_CYCLE);check(!close(fd));}
    }
    closed();source_close(&s);
    check(!rm("H:/SHARED.ZPU"));check(!rm("H:/LEFTDEP.ZPU"));check(!rm("H:/RIGHTDEP.ZPU"));
    {uint8_t status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/DIAMOND.COM",0,&status)&&status==0);}
}
static void metadata_constants(void) {
    check(author("H:/LITERALS.PAS","H:/LITERALS.ZPU",
        "unit Literals;interface const Answer=42;Debt=-123;Mark='Q';Yes=true;\n"
        "Words='Hello ''units''';Rate=-1.25;implementation const Hidden=9;end."));
    check(!rm("H:/LITERALS.PAS"));
    check(author("H:/CONSTCOPY.PAS","H:/CONSTCOPY.ZPU",
        "unit ConstCopy;interface uses Literals;const Plus=Answer;\n"
        "Words=Literals.Words;Rate=-Literals.Rate;implementation end."));
    check(!rm("H:/CONSTCOPY.PAS"));
    client("H:/LITFILE.PAS","uses Literals;var F:Text;begin\n"
        "Assign(F,'H:/RAWCONST.TXT');Rewrite(F);Write(F,Literals.Answer);Close(F)end.");
    client("H:/LITRUN.PAS","uses Literals;var N:Integer;begin N:=0;\n"
        "if(Answer<>42)or(Debt<>-123)or(Mark<>'Q')or not Yes then N:=1 div N;\n"
        "if(Words<>'Hello ''units''')or(Rate<>-1.25)then N:=1 div N;\n"
        "writeln('UNIT-CONSTANTS-OK')end.");
    client("H:/COPYCONST.PAS","uses ConstCopy;var N:Integer;begin N:=0;\n"
        "if(Plus<>42)or(Words<>'Hello ''units''')or(Rate<>1.25)then N:=1 div N;\n"
        "writeln('UNIT-CONSTCOPY-OK')end.");
    check(source_new(&s,"H:/LITHIDE.PAS"));check(load("uses Literals;begin writeln(Hidden)end."));
    check(!compile(&s,&b,&d)&&d.code==41);closed();source_close(&s);
    check(!rm("H:/LITERALS.ZPU"));check(!rm("H:/CONSTCOPY.ZPU"));
    {uint8_t status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/LITRUN.COM",0,&status)&&status==0);}
    {uint8_t status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/COPYCONST.COM",0,&status)&&status==0);}
}
/* Corrupt unused metadata with a correct artifact checksum: structural and
 * semantic rejection must both reclaim pages and restore the warm code set. */
static void malformed_types(void) {
    uint8_t page=page_new(PAGE_COMPILER);uint16_t bytes,at,n,i,j,m,first_record=0,array=0,old;
    check(page!=255);if(page==255)return;
    bytes=read_artifact("H:/KINDS.ZPU",page);at=page_word(page,42);n=page_word(page,38);
    for(i=0;i<n;++i) {
        if(page_word(page,at)==K_RECORD&&!first_record)first_record=at;
        if(page_word(page,at)==K_ARRAY)array=at;
        m=page_word(page,at+16);at+=18;
        for(j=0;j<m;++j)at+=10+far_get(page,at+1);
    }
    check(first_record&&array);
    if(first_record&&array) {
        at=first_record+20;old=page_word(page,at);page_word_set(page,at,TYPE_NONE);
        checksum(page,bytes);write_artifact("H:/KINDS.ZPU",page,bytes);
        check(source_new(&s,"H:/TYPEFORMAT.PAS"));check(load("uses Kinds;begin end."));
        check(!compile(&s,&b,&d)&&d.code==UNIT_FORMAT);closed_extra(1);source_close(&s);
        page_word_set(page,at,old);old=page_word(page,array+2);page_word_set(page,array+2,1);
        checksum(page,bytes);write_artifact("H:/KINDS.ZPU",page,bytes);
        check(source_new(&s,"H:/TYPEWIDTH.PAS"));check(load("uses Kinds;begin end."));
        check(!compile(&s,&b,&d)&&d.code==UNIT_FORMAT);closed_extra(1);source_close(&s);
        page_word_set(page,array+2,old);checksum(page,bytes);write_artifact("H:/KINDS.ZPU",page,bytes);
    }
    check(page_drop(page));
}
static const char typed_unit[]=
    "unit Kinds;interface const Limit=3;type TShade=(ShadeLow,ShadeHigh);\n"
    "TIndex=1..Limit;TPoint=record X,Y:Integer end;\n"
    "TPoints=array[TIndex]of TPoint;TShades=set of TShade;TText=string[12];\n"
    "PNode=^TNode;TNode=record Next:PNode;Kind:TShade end;\n"
    "TChoice=record case Tag:Boolean of false:(I:Integer);true:(R:Real)end;\n"
    "PHeader=^THeader;PLink=^TLink;THeader=record Owner:PLink end;\n"
    "TLink=record Hdr:THeader end;TBin=file of TPoint;function Pick(S:TShade):TShade;\n"
    "procedure Bump(var P:TPoint);procedure Fill(var A:TPoints);\n"
    "procedure Echo(V:TText;var R:TText);implementation\n"
    "function Pick(S:TShade):TShade;begin if S=ShadeLow then Pick:=ShadeHigh else Pick:=ShadeLow end;\n"
    "procedure Bump(var P:TPoint);begin P.X:=P.X+1;P.Y:=P.Y+2 end;\n"
    "procedure Fill(var A:TPoints);begin A[1].X:=10;A[3].Y:=30 end;\n"
    "procedure Echo(V:TText;var R:TText);begin R:=V end;end.";
static void metadata_types(void) {
    check(author("H:/KINDS.PAS","H:/KINDS.ZPU",typed_unit));
    check(!rm("H:/KINDS.PAS"));malformed_types();
    check(source_new(&s,"H:/TYPELEAF.PAS"));
    check(load("uses Kinds;var P:TPoint;begin P.X:=Limit end."));check(compile(&s,&b,&d));
    if(b.valid) {
        check((far_get(b.pages[0],18)&NATIVE_SHARED_STACK)&&b.pages[2]==255&&b.globals_bytes==4);
        check(save_build(&s,&b,0));check(run_build(&s,&b)&&ram_result[26]==1);
    }
    closed();source_close(&s);
    check(author("H:/TYPECOPY.PAS","H:/TYPECOPY.ZPU",
        "unit TypeCopy;interface uses Kinds;type TAlias=TPoint;TShadeAlias=TShade;\n"
        "PQualified=^Kinds.TPoint;TQualified=record Ref:^Kinds.TPoint end;\n"
        "const Chosen=ShadeHigh;procedure Shift(var P:TAlias);\n"
        "function Flip(V:TShadeAlias):TShadeAlias;implementation\n"
        "procedure Shift(var P:TAlias);begin Kinds.Bump(P)end;\n"
        "function Flip(V:TShadeAlias):TShadeAlias;begin Flip:=Kinds.Pick(V)end;end."));
    check(!rm("H:/TYPECOPY.PAS"));
    client("H:/TYPERUN.PAS","uses TypeCopy,Kinds;type PLocal=^Kinds.TPoint;\n"
        "TLocal=record Ref:^Kinds.TPoint end;var P:Kinds.TPoint;A:TPoints;S:TShades;T,U:TText;\n"
        "N:TNode;V:TChoice;F:TBin;C:TypeCopy.TShadeAlias;Z:Integer;\n"
        "Q:PLocal;W:TLocal;Q2:PQualified;W2:TQualified;H:THeader;L:TLink;begin Z:=0;\n"
        "P.X:=40;P.Y:=1;Shift(P);if(P.X<>41)or(P.Y<>3)then Z:=1 div Z;\n"
        "Q:=ptr(addr(P));W.Ref:=Q;Q2:=Q;W2.Ref:=Q2;if(W.Ref^.X<>41)or(W2.Ref^.Y<>3)then Z:=1 div Z;\n"
        "L.Hdr.Owner:=nil;H.Owner:=ptr(addr(L));if H.Owner^.Hdr.Owner<>nil then Z:=1 div Z;\n"
        "Fill(A);if(A[1].X<>10)or(A[3].Y<>30)then Z:=1 div Z;\n"
        "T:='hello';Echo(T,U);if U<>'hello'then Z:=1 div Z;\n"
        "S:=[ShadeLow];S:=S+[ShadeHigh];if not(Chosen in S)then Z:=1 div Z;\n"
        "C:=Flip(ShadeLow);if C<>ShadeHigh then Z:=1 div Z;\n"
        "N.Next:=ptr(addr(N));N.Kind:=C;if N.Next^.Kind<>ShadeHigh then Z:=1 div Z;\n"
        "V.Tag:=false;V.I:=123;if(V.I<>123)or(SizeOf(V)<>7)then Z:=1 div Z;\n"
        "V.Tag:=true;V.R:=1.5;if V.R<>1.5 then Z:=1 div Z;\n"
        "if(SizeOf(F)<>176)or(SizeOf(TPoint)<>4)then Z:=1 div Z;\n"
        "writeln('UNIT-TYPES-OK')end.");
    check(author("H:/OTHERKINDS.PAS","H:/OTHERKINDS.ZPU",
        "unit OtherKinds;interface type TShade2=(OtherLow,OtherHigh);implementation end."));
    check(source_new(&s,"H:/TYPEBAD.PAS"));
    check(load("uses Kinds,OtherKinds;var A:Kinds.TShade;B:OtherKinds.TShade2;begin A:=B end."));
    check(!compile(&s,&b,&d)&&d.code==44);closed();source_close(&s);
    check(source_new(&s,"H:/BADQUAL.PAS"));
    check(load("uses Kinds;type R=record X:Integer end;P=^R.NotAType;begin end."));
    check(!compile(&s,&b,&d)&&d.code==42);closed();source_close(&s);
    check(!rm("H:/KINDS.ZPU"));check(!rm("H:/TYPECOPY.ZPU"));check(!rm("H:/OTHERKINDS.ZPU"));
    {uint8_t status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/TYPERUN.COM",0,&status)&&status==0);}
    {uint8_t status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/TYPELEAF.COM",0,&status)&&status==0);}
}
static void variant_roundtrip(void) {
    check(author("H:/TAGS.PAS","H:/TAGS.ZPU",
        "unit Tags;interface type Small=1..3;Wide=record case Byte of 300:(A:Integer)end;\n"
        "Narrow=record case Small of 5:(B:Integer)end;implementation end."));
    check(!rm("H:/TAGS.PAS"));
    client("H:/TAGSUSE.PAS","uses Tags;var W:Wide;N:Narrow;Z:Integer;begin Z:=0;\n"
        "W.A:=7;N.B:=9;if W.A+N.B<>16 then Z:=1 div Z;writeln('UNIT-VARIANTS-OK')end.");
    check(!rm("H:/TAGS.ZPU"));
    {uint8_t status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/TAGSUSE.COM",0,&status)&&status==0);}
}
int app_main(void) {
    uint8_t status;int8_t capture;
    if(!console_open())return 1;
    memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=b.sources.page=255;
    test_stage=1;io_pressure();missing_main();
    if(!author("H:/MIXED.PAS","H:/MIXED.ZPU",mixed))goto done;
    artifact_negatives();
    check(!rm("H:/MIXED.PAS"));
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
    fault_location();page_pressure(1);page_pressure(0);
    layout_helpers();layout_limits();
    client("H:/SHARED.PAS","var N:Integer;begin N:=21*2;if N<>42 then halt end.");
    execute_shared();argument_calls();dependency_units();metadata_constants();metadata_types();variant_roundtrip();
    test_stage=3;check(!rm("H:/MIXED.ZPU"));check(!rm("H:/MIXED.PAS"));
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/SHARED.COM",0,&status)&&status==0);
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/SHARED.COM",0,&status)&&status==0);
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/SCALE.COM",0,&status)&&status==0);
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/TRANSIT.COM",0,&status)&&status==0);
    status=255;check(!exec(EXEC_PRESERVE_PROGRAM,"H:/FAULTRUN.COM",0,&status)&&status==2);
    if(capture>=0){check(!swap(capture,DEV_STDOUT));check(!close(capture));}
done:
    check(pages_used()==15);console_close();test_done=255;test_complete();return 0;
}
