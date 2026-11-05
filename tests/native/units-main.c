#include "harness.h"
#include "compiler_units.h"
#include "output_transport.h"

volatile uint16_t test_passed,test_failed,test_stage,test_index,test_failures[64];
volatile uint8_t test_done,unit_diagnostic,unit_authors,unit_codes[64];
Source s;Build b;Diagnostic d;
void test_complete(void);

static uint8_t seed_unit(const char *path,const char *text) {
    uint8_t bytes[64];uint16_t at=0,n,i;uint8_t ok=1;int8_t fd;
    rm(path);fd=open(path,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0)return 0;
    while(text[at]&&ok) {
        for(n=0;n<sizeof(bytes)&&text[at+n];++n)bytes[n]=(uint8_t)text[at+n];
        i=n;ok=write(fd,bytes,&i)==0&&i==n;at+=n;
    }
    return close(fd)==0&&ok;
}
static uint8_t copy_named(const char *from,const char *to) {
    char a[128],z[128];strcpy(a,from);strcpy(z,to);return output_copy_file(a,z);
}
static void reject(const char *text,uint8_t expected) {
    check(load(text));check(!compile(&s,&b,&d)&&d.code==expected&&!b.valid);
    unit_diagnostic=d.code;build_close(&b);check(pages_used()==code_required+1);
}
uint8_t unit_bad_author(const char *text);
static void unsupported(const char *declarations,const char *body) {
    char text[512];strcpy(text,"unit BadUnit;interface procedure P;implementation ");
    strcat(text,declarations);strcat(text," procedure P;begin ");strcat(text,body);strcat(text," end;end.");
    unit_bad_author(text);
}
static const char client[]="uses TileMath,Offsets;var N:Integer;begin\n"
    "N:=TileMath.TileOffset(2,3);if N<>242 then halt;\n"
    "if Offsets.AddOne(N)<>243 then halt;writeln('PASCAL-UNITS-OK')end.";
static uint8_t author_loaded(const char *path,const char *output,uint8_t expected) {
    Source source;Build scratch;Diagnostic diagnostic;uint8_t ok,owned=pages_used();int8_t fd;
    check(read_block(&source,path,&diagnostic));
    if(source.page==255)return 0;
    rm(output);fd=open(output,O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);if(fd<0){source_close(&source);return 0;}
    memset(&scratch,0,sizeof(scratch));memset(scratch.pages,255,3);scratch.map_page=scratch.sources.page=255;
    ok=compile_unit(&source,&scratch,&diagnostic,fd,0);unit_diagnostic=diagnostic.code;
    if(unit_authors<64)unit_codes[unit_authors++]=diagnostic.code;
    check((expected?!ok&&diagnostic.code==expected:ok&&!diagnostic.code)&&!scratch.valid);
    check(close(fd)==0);build_close(&scratch);source_close(&source);
    check(pages_used()==owned);return ok;
}
static uint8_t author(const char *path,const char *output,const char *text,uint8_t expected) {
    check(seed_unit(path,text));return author_loaded(path,output,expected);
}
static const char fault_source[]="unit Faults;interface function Fault(X:Integer):Integer;implementation\n"
    "function Fault(X:Integer):Integer;begin Fault:=1 div X end;end.";
static void fault_location(void) {
    uint16_t pc,position;uint8_t file,status;Source view;
    author("H:/FAULTS.PAS","H:/FAULTS.ZPU",fault_source,0);
    check(load("uses Faults;var N:Integer;begin N:=Fault(0)end."));check(compile(&s,&b,&d));
    if(b.valid) {
        check(save_build(&s,&b,0));check(run_build(&s,&b)&&ram_result[26]==3&&ram_result[28]==2);
        pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
        check(build_location(&s,&b,pc,&file,&position)&&file==1);
        check(sources_get(&b.sources,file,&view)&&!strcmp(view.path,"H:/FAULTS.PAS"));
        check(position==(uint16_t)(strstr(fault_source,"Fault:=")-fault_source));
        status=255;check(run_saved(&s,&b,&status)&&status==2);
    }
    build_close(&b);check(pages_used()==code_required+1);
}
uint8_t unit_bad_author(const char *text) {
    return author("H:/BADUNIT.PAS","H:/BADUNIT.ZPU",text,UNIT_UNSUPPORTED);
}
int app_main(void) {
    uint8_t ok,status;int8_t capture=-1;char expanded[1024];uint8_t i;
    if(!console_open())return 1;
    memset(b.pages,255,3);b.map_page=b.sources.page=255;
    check(source_new(&s,"H:/SLICE.PAS"));check(code_capture(0));
    test_stage=1;
    ok=author("H:/TILEMATH.PAS","H:/TILEMATH.ZPU",
        "unit TileMath;interface\n"
        "function TileOffset(X,Y:Integer):Integer;\n"
        "function Unused(X:Integer):Integer;implementation\n"
        "function TileOffset(X,Y:Integer):Integer;\n"
        "begin TileOffset:=Y*80+X end;\n"
        "function Unused(X:Integer):Integer;begin Unused:=X+999 end;end.",0);
    if(!ok)goto done;
    test_stage=2;
    ok=author("H:/OFFSETS.PAS","H:/OFFSETS.ZPU",
        "unit Offsets;interface function AddOne(X:Integer):Integer;implementation\n"
        "function AddOne(X:Integer):Integer;begin AddOne:=X+1 end;end.",0);
    if(!ok)goto done;
    test_stage=3;
    author("H:/BADUNIT.PAS","H:/BADUNIT.ZPU",
        "unit BadUnit;interface function F:Real;implementation function F:Real;begin F:=1 end;end.",UNIT_UNSUPPORTED);
    author("H:/BADUNIT.PAS","H:/BADUNIT.ZPU",
        "unit BadUnit;interface procedure P(X:Integer);implementation procedure P(X:Byte);begin end;end.",UNIT_INTERFACE);
    author("H:/BADUNIT.PAS","H:/BADUNIT.ZPU",
        "unit BadUnit;interface procedure P;implementation end.",73);
    author("H:/BADUNIT.PAS","H:/BADUNIT.ZPU",
        "unit BadUnit;interface implementation begin end.",UNIT_UNSUPPORTED);
    capture=open("H:/UNITS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=53;
    execution(client,1,0);
    check(copy_named("H:/SLICE.COM","H:/SMALL.COM"));
    check(copy_named("H:/TILEMATH.ZPU","H:/SMALL.ZPU"));
    strcpy(expanded,"unit TileMath;interface\nfunction TileOffset(X,Y:Integer):Integer;\n"
        "function Unused(X:Integer):Integer;implementation\nfunction TileOffset(X,Y:Integer):Integer;\n"
        "begin TileOffset:=Y*80+X end;\nfunction Unused(X:Integer):Integer;begin ");
    for(i=0;i<20;++i)strcat(expanded,"Unused:=X+1;\n");
    strcat(expanded,"Unused:=X+999 end;end.");
    author("H:/TILEMATH.PAS","H:/TILEMATH.ZPU",expanded,0);
    execution(client,1,0);check(copy_named("H:/SLICE.COM","H:/LARGE.COM"));
    check(copy_named("H:/TILEMATH.ZPU","H:/LARGE.ZPU"));
    /* Direct SDK execution has no compiler source/readiness checks. */
    check(rm("H:/TILEMATH.ZPU")==0);check(rm("H:/OFFSETS.ZPU")==0);
    check(rm("H:/TILEMATH.PAS")==0);check(rm("H:/OFFSETS.PAS")==0);
    status=255;check(exec(EXEC_PRESERVE_PROGRAM,"H:/SLICE.COM",0,&status)==0&&status==0);
    check(pages_used()==code_required+1);
    author("H:/VARS.PAS","H:/VARS.ZPU",
        "unit Vars;interface procedure Bump(var X:Integer);implementation procedure Bump(var X:Integer);begin X:=X+1 end;end.",0);
    execution("uses Vars;var N:Integer;begin N:=41;Bump(N);if N<>42 then halt;writeln('UNIT-VAR-OK')end.",1,0);
    reject("uses Vars;begin Vars.Missing end.",UNIT_INTERFACE);
    reject("uses Vars;\n{$I VARS.PAS}\nbegin end.",93);
    reject("uses Missing;begin end.",UNIT_MISSING);
    check(seed_unit("H:/VARS.PAS","changed"));reject("uses Vars;begin end.",UNIT_STALE);
    check(seed_unit("H:/VARS.ZPU","bad"));reject("uses Vars;begin end.",UNIT_FORMAT);
    author("H:/LEFT.PAS","H:/LEFT.ZPU",
        "unit Left;interface function Clash:Integer;implementation function Clash:Integer;begin Clash:=1 end;end.",0);
    author("H:/RIGHT.PAS","H:/RIGHT.ZPU",
        "unit Right;interface function Clash:Integer;implementation function Clash:Integer;begin Clash:=2 end;end.",0);
    reject("uses Left,Right;begin writeln(Clash)end.",UNIT_AMBIGUOUS);
    execution("uses Left,Right;function Clash:Integer;begin Clash:=9 end;"
        "begin if Clash<>9 then halt;writeln('UNIT-SHADOW-OK')end.",1,0);
    author("H:/BADUNIT.PAS","H:/BADUNIT.ZPU",
        "unit BadUnit;interface procedure P;procedure P;implementation procedure P;begin end;end.",UNIT_INTERFACE);
    unsupported("","BufLen:=1");unsupported("","HeapPtr:=0");unsupported("","RecurPtr:=0");
    unsupported("var X:^Integer;","new(X)");unsupported("var X:^Integer;","mark(X)");
    unsupported("var X:^Integer;","release(X)");unsupported("","write('x')");
    unsupported("var C:Char;","read(C)");
    unsupported("","if KeyPressed then begin end");
    unsupported("var F:Text;","assign(F,'H:/X')");
    unsupported("var N:Integer;","N:=random(3)");
    unsupported("","inline($00)");unsupported("","randomize");unsupported("","clrscr");
    unsupported("var A:Integer absolute $100;","A:=1");
    unsupported("","ConOutPtr:=0");unsupported("","CBreak:=true");
    unsupported("procedure Q(BufLen:Integer);begin end;\n","BufLen:=1");
    author("H:/KINDS.PAS","H:/KINDS.ZPU",
        "unit Kinds;interface\nprocedure Change(var B:Byte;var C:Char;var F:Boolean);\n"
        "function Flip(F:Boolean):Boolean;implementation\nconst Delta:Integer=1;\n"
        "procedure Change(var B:Byte;var C:Char;var F:Boolean);\n"
        "begin B:=B+Delta;C:=chr(ord(C)+Delta);F:=not F end;\n"
        "function Flip(F:Boolean):Boolean;begin Flip:=not F end;end.",0);
    execution("uses Kinds;var B:Byte;C:Char;F:Boolean;begin\n"
        "B:=41;C:='A';F:=false;Change(B,C,F);\n"
        "if(B<>42)or(C<>'B')or(not F)or Flip(F)then halt;\n"
        "writeln('UNIT-KINDS-OK')end.",1,0);
    author_loaded("H:/PRIVATE.PAS","H:/PRIVATEOPS.ZPU",0);
    execution("uses PrivateOps;var N:Integer;begin\n"
        "if Exercise(5)<>46 then halt;if Exercise(7)<>50 then halt;\n"
        "N:=41;PrivateOps.ShadowHook(N);if N<>42 then halt;\n"
        "if EchoByte(255)<>255 then halt;if EchoChar('Z')<>'Z' then halt;\n"
        "Untyped(N);if N<>17 then halt;writeln('UNIT-PRIVATE-OK')end.",1,0);
    check(copy_named("H:/SLICE.COM","H:/PRIVATE.COM"));
    fault_location();
    unit_diagnostic=d.code;
done:
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    check(code_capture(1));build_close(&b);source_close(&s);
    check(pages_used()==code_required);
    test_done=255;test_complete();return 0;
}
