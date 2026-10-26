#include "library.h"
#include <zos_vfs.h>
#include <zos_sys.h>

/* Test executable runs on the unmodified pinned kernel/emulator. The headless
 * driver reads this bounded result block through the emulator's supported
 * debugger console. No emulated syscall or host Pascal interpreter is used. */
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
/* Executed only after completion. Pascal code/static ends below C000, so an
 * address-only debugger stop here cannot match any generated runtime helper. */
uint8_t test_stop[3]={0x76,0x18,0xfd};
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
static Source s;
static Build b;
static Diagnostic d;
static uint8_t initial_code_hash[32];
extern uint8_t console_scratch[256];
void test_machine(uint8_t *state) NATIVE;
void test_complete(void);
static void check(uint8_t ok) {
    ++test_index;
    if(ok) ++test_passed;
    else { if(test_failed<64) test_failures[test_failed]=test_index; ++test_failed; }
}
static void code_hash(uint8_t *out) {
    Sha256 hash; uint8_t bank; uint16_t i;
    sha_init(&hash);
    for(bank=0;bank<code_required;++bank)
        for(i=0;i<code_sizes[bank];++i) sha_byte(&hash,far_get(code_pages[bank],i));
    sha_end(&hash,out);
}
static uint8_t load(const char *text) { return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d); }
static uint8_t seed(const char *path,const char *text) {
    int8_t fd;
    uint16_t n=(uint16_t)strlen(text),expected=n;
    strcpy((char*)console_scratch,path); fd=open((char*)console_scratch,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0) return 0;
    strcpy((char*)console_scratch,text);
    if(write(fd,console_scratch,&n)||n!=expected) { close(fd); return 0; }
    return close(fd)==0;
}
/* Publication and saved execution must select the same ordinary corpora. */
static uint8_t saved_corpus(void) {
    switch(test_stage) {
    case 9: case 11: case 13: case 15: case 17: case 19:
    case 21: case 23: case 25: case 27: case 29: case 31: case 33: case 35: case 37: return 1;
    default: return 0;
    }
}
static void execute_loaded(uint8_t kind,uint8_t code,uint16_t expected_position) {
    uint8_t owned;
    uint8_t before[7],after[7];
    uint16_t pc,position;
    uint32_t revision;
    s.dirty=1; revision=s.revision;
    if(test_stage==3) { __asm di __endasm; }
    test_machine(before);
    check(compile(&s,&b,&d));
    test_machine(after);
    check(!memcmp(before,after,7)); /* compiler bank, stack and IFF restored */
    if(!b.valid) { if(test_stage==3) { __asm ei __endasm; } return; }
    if(test_stage==2) {
        check(seed("H:/SLICE.COM","old executable")); check(seed("H:/SLICE.ZMP","old map"));
        check(save_build(&s,&b));
    }
    if(test_stage==7||saved_corpus()) check(save_build(&s,&b));
    owned=pages_used();
    test_machine(before);
    check(run_build(&s,&b));
    test_machine(after);
    if(test_stage==3) { __asm ei __endasm; }
    check(!memcmp(before,after,7));
    check(!memcmp(before+3,ram_result+8,4));
    check(before[0]==ram_result[15]);
    check(ram_result[26]==kind&&ram_result[28]==code);
    check(s.dirty&&s.revision==revision&&pages_used()==owned);
    if(kind==3) {
        pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
        check(build_locate(&s,&b,pc,&position));
        check(position==expected_position);
    }
    check(build_check(&s,&b)); /* writable run pages cannot mutate cached image */
    if(test_stage==2) {
        uint8_t status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/SLICE.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);
        status=255; check(run_saved(&s,&b,&status)&&status==0);
        check(pages_used()==owned);
    }
    if(test_stage==7) {
        uint8_t status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/NAME.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);
    }
    if(saved_corpus()) {
        uint8_t status=255;
        check(run_saved(&s,&b,&status)&&status==0); check(pages_used()==owned);
    }
    build_close(&b);
    check(pages_used()==code_required+1);
}
static void execution(const char *text,uint8_t kind,uint8_t code) {
    check(load(text)); execute_loaded(kind,code,6);
}
static void execution_file(const char *path,uint8_t kind,uint8_t code,uint16_t expected_position) {
    check(load_source(&s,path,&d)); execute_loaded(kind,code,expected_position);
}
static void control_tests(void) {
    int8_t capture=open("H:/CONTROL.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=9; execution_file("H:/CONTROL.PAS",1,0,0);
    test_stage=10;
    execution_file("H:/EAGERAND.PAS",3,2,6);
    execution_file("H:/EAGEROR.PAS",3,2,6);
    execution_file("H:/NESTED.PAS",3,2,34);
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void checked_error(const char *text,uint8_t code,const char *where) {
    uint16_t position=(uint16_t)(strstr(text,where)-text);
    check(load(text)); execute_loaded(3,code,position);
}
static void ordinal_tests(void) {
    int8_t capture=open("H:/ORDINAL.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=11;
    execution_file("H:/ORDINALS.PAS",1,0,0);
    execution_file("H:/ENUM256.PAS",1,0,0);
    execution_file("H:/ENUM257.PAS",1,0,0);
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
    test_stage=12;
    checked_error("var b:byte; begin\n{$R+} b:=256 end.",0x91,"b:=256");
    checked_error("type t=-2..2; var n:t; begin\n{$R+} n:=-3 end.",0x91,"n:=-3");
    checked_error("type t=(red,green,blue); var c:t; begin\n{$R+} c:=t(3) end.",0x91,"c:=t(3)");
    checked_error("var b:byte; begin b:=255;\n{$R+} b:=succ(b) end.",0x91,"b:=succ");
    checked_error("var c:char; s:string[2]; begin s:='ab';\nc:=s end.",0x10,"c:=s");
    checked_error("var c:char; s:string[2]; begin s:='';\nc:=s end.",0x10,"c:=s");
    checked_error("type t=1..3; var i:t; begin\n{$R+} for i:=0 to 3 do end.",0x91,"for i");
}
static void case_tests(void) {
    int8_t capture=open("H:/CASES.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=13; execution_file("H:/CASES.PAS",1,0,0);
    test_stage=14;
    checked_error("begin\ncase 7 div 0 of 1: end end.",2,"case");
    checked_error("begin case 1 of\n1:writeln(7 div 0); else writeln(0) end end.",2,"writeln(7");
    checked_error("begin case 1 of\n2:; else writeln(7 div 0) end end.",2,"writeln(7");
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void routine_tests(void) {
    int8_t capture=open("H:/ROUTINES.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=15; execution_file("H:/ROUTINES.PAS",1,0,0);
    test_stage=16;
    checked_error("{$A-} procedure dive; var scratch:string[255]; begin dive end; begin dive end.",0xff,"dive");
    checked_error("function fail(x:integer):integer; begin fail:=7 div x end; begin writeln(fail(0)) end.",2,"fail:=");
    checked_error("procedure p(x:byte); begin end; begin {$R+} p(256) end.",0x91,"p(256)");
    execution("procedure p; begin halt end; begin p end.",4,0);
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void string_tests(void) {
    char tight[512];
    uint16_t n;
    uint8_t i;
    int8_t capture=open("H:/SRESULT.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=17;
    execution_file("H:/STRINGFN.PAS",1,0,0);
    execution_file("H:/STRLIFE.PAS",1,0,0);
    execution_file("H:/STRBOUND.PAS",1,0,0);
    execution_file("H:/STRINGABI.PAS",1,0,0);
    test_stage=18;
    checked_error("type text=string[3]; var c:char; function bad:text; begin bad:='ab' end; begin\nc:=bad end.",0x10,"c:=bad");
    checked_error("var s:string[255]; t:string[1]; begin\ns:='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-';\ns:=s+s;\nt:=s+s end.",0x10,"t:=s+s");
    strcpy(tight,"{$A-} type big=string[255]; function value:big; begin value:='' end; procedure tight; var\n");
    for(i=0;i<51;++i) {
        n=(uint16_t)strlen(tight); tight[n++]='p'; tight[n++]='a'+i/26;
        tight[n++]='a'+i%26; tight[n++]=i<50?',':':'; tight[n++]='\n'; tight[n]=0;
    }
    strcat(tight,"big; begin writeln(value) end; begin tight end.");
    checked_error(tight,0xff,"writeln(value)");
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void string_operation_tests(void) {
    int8_t capture=open("H:/STROPS.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=19;
    execution_file("H:/STRINDEX.PAS",1,0,0);
    execution_file("H:/STROPS.PAS",1,0,0);
    execution_file("H:/STROP255.PAS",1,0,0);
    test_stage=20;
    checked_error("var s:string[3]; begin s:='abc'; {$R+}\ns[-1]:='x' end.",0x90,"s[-1]");
    checked_error("var s:string[3]; begin s:='abc'; {$R+}\nwriteln(s[4]) end.",0x90,"writeln");
    checked_error("var s:string[3]; begin s:='abc'; {$R+}\ns[4]:='x' end.",0x90,"s[4]");
    checked_error("var s:string[3]; begin s:='abc'; {$R+}\nwriteln(s[-1]) end.",0x90,"writeln");
    checked_error("begin\nwriteln(copy('abc',0,1)) end.",0x11,"writeln");
    checked_error("begin\nwriteln(copy('abc',256,0)) end.",0x11,"writeln");
    checked_error("begin\nwriteln(copy('abc',1,-1)) end.",0x11,"writeln");
    checked_error("var s:string[3]; begin s:='abc';\ndelete(s,0,0) end.",0x11,"delete");
    checked_error("var s:string[3]; begin s:='abc';\ndelete(s,1,-1) end.",0x11,"delete");
    checked_error("var s:string[3]; begin s:='abc';\ninsert('',s,256) end.",0x11,"insert");
    checked_error("var s:string[3]; begin s:='abc';\ninsert('x',s,-1) end.",0x11,"insert");
    checked_error("var s:string[255]; begin s:='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-'; s:=s+s;\ns:=concat(s,s) end.",0x10,"s:=concat");
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void array_tests(void) {
    int8_t capture=open("H:/ARRAYS.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=21;
    execution_file("H:/ARRAYS.PAS",1,0,0);
    execution_file("H:/ARRAYARG.PAS",1,0,0);
    execution_file("H:/CHARARR.PAS",1,0,0);
    test_stage=22;
    checked_error("var a:array[-2..2] of byte; begin {$R+}\na[-3]:=1 end.",0x90,"a[-3]");
    checked_error("var a:array[-2..2] of byte; begin {$R+}\nwriteln(a[3]) end.",0x90,"writeln");
    checked_error("var a:array[boolean,1..2] of byte; begin {$R+}\na[true,3]:=1 end.",0x90,"a[true");
    checked_error("var a:array[1..2] of byte; begin {$R+}\na[1]:=256 end.",0x91,"a[1]");
    checked_error("var a:array[1..2] of string[3]; c:char; begin a[1]:='ab';\nc:=a[1] end.",0x10,"c:=a");
    checked_error("{$A-} procedure dive; var a:array[1..255] of integer; begin dive end; begin dive end.",0xff,"dive");
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void record_tests(void) {
    int8_t capture=open("H:/RECORDS.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=23;
    execution_file("H:/RECORDS.PAS",1,0,0);
    execution_file("H:/VARIANTS.PAS",1,0,0);
    execution_file("H:/WITHREC.PAS",1,0,0);
    execution_file("H:/ZEROREC.PAS",1,0,0);
    test_stage=24;
    checked_error("type r=record b:byte end;var x:r;begin {$R+}\nx.b:=256 end.",0x91,"x.b:=");
    checked_error("type r=record s:string[3] end;var x:r;begin {$R+}\nx.s[4]:='a' end.",0x90,"x.s[4]");
    checked_error("type r=record a:array[1..2] of byte end;var x:r;begin {$R+}with x do\na[3]:=0 end.",0x90,"a[3]");
    checked_error("type r=record n:integer end;var x:r;begin with x do\nn:=7 div 0 end.",2,"n:=7");
    checked_error("type r=record s:string[3] end;var x:r;c:char;begin x.s:='ab';with x do\nc:=s end.",0x10,"c:=s");
    checked_error("{$A-}type r=record s:string[255] end;procedure dive;var own:r;begin with own do dive end;begin dive end.",0xff,"dive");
    execution("type r=record n:integer end;var x:r;procedure p;begin with x do halt end;begin p end.",4,0);
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void set_tests(void) {
    int8_t capture=open("H:/SETS.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=25;
    execution_file("H:/SETS.PAS",1,0,0);
    execution_file("H:/SETSHAPE.PAS",1,0,0);
    execution_file("H:/SETARGS.PAS",1,0,0);
    execution_file("H:/SETABI.PAS",1,0,0);
    test_stage=26;
    checked_error("var s:set of byte;begin\ns:=[1,7 div 0]end.",2,"s:=[");
    checked_error("var s:set of byte;begin\ns:=[1..7 div 0]end.",2,"s:=[");
    checked_error("var s:set of byte;begin s:=[1];\nif (7 div 0) in s then end.",2,"if");
    checked_error("var s:set of byte;b:byte;begin b:=255;{$R+}\ns:=[succ(b)]end.",0x91,"s:=[");
    checked_error("{$A-}type t=set of byte;procedure tight(v:t);var a:array[1..14000]of byte;begin end;begin tight([])end.",0xff,"tight");
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void invalid_ordinal(const char *text) {
    uint8_t before[7],after[7],hash[32],now[32];
    check(load(text)); source_hash(&s,hash);
    test_machine(before);
    check(!compile(&s,&b,&d)&&d.code==31);
    test_machine(after); source_hash(&s,now);
    check(!memcmp(before,after,7));
    check(!memcmp(hash,now,32)&&pages_used()==code_required+1);
}
static void pointer_tests(void) {
    int8_t capture=open("H:/POINTERS.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=27;
    execution_file("H:/POINTERS.PAS",1,0,0);
    execution_file("H:/PTRARGS.PAS",1,0,0);
    execution_file("H:/ORDCHAR.PAS",1,0,0);
    test_stage=28;
    checked_error("begin\nwriteln(ord('ab'))end.",0x10,"writeln");
    checked_error("begin\nwriteln(succ(''))end.",0x10,"writeln");
    checked_error("begin\nwriteln(pred('ab'))end.",0x10,"writeln");
    invalid_ordinal("begin writeln(succ(nil))end.");
    invalid_ordinal("begin writeln(pred(nil))end.");
    invalid_ordinal("begin writeln(succ(ptr(0)))end.");
    invalid_ordinal("begin writeln(pred(ptr(0)))end.");
    invalid_ordinal("begin writeln(succ([]))end.");
    invalid_ordinal("begin writeln(pred([1]))end.");
    checked_error("type p=^byte;var x:p;b:byte;begin x:=ptr(addr(b));{$R+}\nx^:=256 end.",0x91,"x^:=");
    checked_error("type a=array[1..2]of byte;p=^a;var x:p;v:a;begin x:=ptr(addr(v));{$R+}\nx^[3]:=0 end.",0x90,"x^[3]");
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void heap_tests(void) {
    int8_t capture=open("H:/HEAP.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=29;
    execution_file("H:/HEAPMARK.PAS",1,0,0);
    execution_file("H:/HEAPFREE.PAS",1,0,0);
    execution_file("H:/POINTERABI.PAS",1,0,0);
    test_stage=30;
    checked_error("type p=^integer;var x:p;begin\ngetmem(x,-1)end.",0xff,"getmem");
    checked_error("type p=^integer;var x:p;begin\nwhile true do getmem(x,1024)end.",0xff,"getmem");
    checked_error("type p=^integer;var x:p;begin heapptr:=0;\nnew(x)end.",0xff,"new");
    checked_error("type p=^integer;var x:p;begin x:=nil;\nrelease(x)end.",0xff,"release");
    checked_error("type p=^integer;var x:p;begin new(x);dispose(x);\ndispose(x)end.",0xff,"dispose(x)end");
    checked_error("type p=^integer;var x:p;begin getmem(x,10);\nfreemem(x,8)end.",0xff,"freemem");
    checked_error("{$A-}type p=^integer;var x:p;procedure tight;var a:array[1..512]of byte;begin end;begin getmem(x,maxavail);tight end.",0xff,"tight");
    checked_error("type p=^integer;var x:p;s:string[255];begin s:='a';getmem(x,maxavail-64);\nwriteln(s+'')end.",0xff,"writeln");
    checked_error("type p=^integer;text=string[255];var x:p;s:text;\nprocedure accept(v:text);begin end;\nbegin s:='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-';\ns:=s+s;getmem(x,maxavail-64);\naccept(s)end.",0xff,"accept(s)");
    checked_error("begin\nstackptr:=0 end.",0xff,"stackptr");
    checked_error("begin\nstackptr:=$C7FF end.",0xff,"stackptr");
    checked_error("begin\nstackptr:=$FC01 end.",0xff,"stackptr");
    checked_error("begin\nstackptr:=$FFFF end.",0xff,"stackptr");
    execution("begin stackptr:=$C800;halt end.",4,0);
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void memory_tests(void) {
    int8_t capture=open("H:/MEMORY.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=31;
    execution_file("H:/ABSOLUTE.PAS",1,0,0);
    execution_file("H:/ABSFRAME.PAS",1,0,0);
    execution_file("H:/RAWMEMORY.PAS",1,0,0);
    execution_file("H:/NATIVEIO.PAS",1,0,0);
    test_stage=32;
    checked_error("var n:integer;b:byte absolute n;begin {$R+}\nb:=256 end.",0x91,"b:=");
    checked_error("var n:integer;b:array[0..1]of byte absolute n;begin {$R+}\nb[2]:=0 end.",0x90,"b[2]");
    checked_error("var n:integer;begin {$R+}\nmem[addr(n)]:=256 end.",0x91,"mem[");
    checked_error("var n:integer;begin\nmem[7 div 0]:=1 end.",2,"mem[");
    checked_error("var a,b:byte;begin\nmove(a,b,7 div 0)end.",2,"move");
    checked_error("var a:byte;begin\nfillchar(a,7 div 0,0)end.",2,"fillchar");
    checked_error("var a:byte;begin\nfillchar(a,1,'ab')end.",0x10,"fillchar");
    checked_error("var a:byte;begin\nfillchar(a,0,'')end.",0x10,"fillchar");
    checked_error("var a:array[1..2]of byte;begin {$R+}\nmove(a[3],a[1],0)end.",0x90,"move");
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void label_tests(void) {
    int8_t capture=open("H:/LABELS.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=33;
    execution_file("H:/GOTOS.PAS",1,0,0);
    test_stage=34;
    checked_error("label l;begin goto l;l:\nwriteln(7 div 0)end.",2,"writeln");
    checked_error("label l;var i:integer;begin for i:=1 to 3 do goto l;l:\nwriteln(7 div 0)end.",2,"writeln");
    checked_error("label l;var r:record n:integer end;begin with r do goto l;l:\nwriteln(7 div 0)end.",2,"writeln");
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void real_tests(void) {
    int8_t capture=open("H:/REALS.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=35;
    execution_file("H:/REALS.PAS",1,0,0);
    execution_file("H:/REALABI.PAS",1,0,0);
    test_stage=36;
    checked_error("var r:real;begin\nr:=1/0 end.",2,"r:=");
    checked_error("var r:real;begin\nr:=1.0/0.0 end.",2,"r:=");
    checked_error("var r:real;begin\nr:=1.0e37*20 end.",1,"r:=");
    checked_error("var r:real;begin\nr:=sqr(1.0e37)end.",1,"r:=");
    checked_error("begin\nwriteln(round(32767.5))end.",0x92,"writeln");
    checked_error("begin {$R-}\nwriteln(trunc(40000.0))end.",0x92,"writeln");
    checked_error("begin {$R+}\nwriteln(trunc(-40000.0))end.",0x92,"writeln");
    checked_error("var r:real;begin heapptr:=stackptr-128;\nr:=1.0+2.0 end.",0xff,"r:=1.0");
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void format_tests(void) {
    int8_t capture=open("H:/FORMATS.TXT",O_WRONLY|O_CREAT|O_TRUNC),input;
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=37;
    execution_file("H:/FORMATOUT.PAS",1,0,0);
    execution_file("H:/STRFORMAT.PAS",1,0,0);
    execution_file("H:/VALPARSE.PAS",1,0,0);
    test_stage=38;
    checked_error("begin\nwriteln(1.25:(7 div 0):2)end.",2,"writeln");
    checked_error("var s:string[8];begin\nstr(1.25:8:round(40000.0),s)end.",0x92,"str(");
    checked_error("var a:array[1..2]of string[8];begin\nstr(1.25:8:2,a[7 div 0])end.",2,"str(");
    checked_error("begin heapptr:=stackptr-80;\nwriteln(1.25)end.",0xff,"writeln");
    checked_error("var i,c:integer;begin heapptr:=stackptr-80;\nval('1',i,c)end.",0xff,"val(");
    input=open("H:/FORMATOUT.PAS",O_RDONLY);check(input>=0);
    if(input>=0) {
        check(swap(input,DEV_STDOUT)==0);
        checked_error("begin\nwriteln(1.25:10:2)end.",0xf0,"writeln");
        check(swap(input,DEV_STDOUT)==0);check(close(input)==0);
    }
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}
static void media_tests(void) {
    uint8_t attempt,status;
    int8_t capture;
    test_stage=8;
    strcpy(s.path,"B:/UNTITLED.PAS");
    check(load("begin\nwriteln('MEDIA')\nend.\n"));
    check(mkdir("B:/ZP0000")==0);
    check(seed("B:/ZP0000/KEEP.PAS","unrelated recovery"));
    capture=open("H:/MEDIA.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    for(attempt=0;attempt<3;++attempt) {
        s.dirty=1;
        check(save_source(&s,1)&&!s.dirty&&!*output_recovery());
        check(compile(&s,&b,&d));
        if(!b.valid) break;
        check(save_build(&s,&b)&&!*output_recovery());
        status=255; check(run_saved(&s,&b,&status)&&status==0);
        if(attempt<2) build_close(&b);
    }
    strcpy(s.path,"B:/UNTITLED.COM");
    check(!save_build(&s,&b)&&platform_error==ERR_INVALID_PATH);
    strcpy(s.path,"B:/UNTITLED.BAK");
    check(!save_source(&s,1)&&platform_error==ERR_INVALID_PATH);
    strcpy(s.path,"B:/UNTITLED.PAS");
    status=255; check(run_saved(&s,&b,&status)&&status==0);
    build_close(&b);
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
    check(pages_used()==code_required+1);
}
int app_main(void) {
    uint8_t digest[32],expected[32]={
        0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    Sha256 hash;
    uint8_t line[129]; uint16_t i;
    int8_t capture;
    if(!console_open()) { test_failed=1; test_done=255; for(;;) {} }
    check(pages_used()==code_required);
    code_hash(initial_code_hash);
    test_stage=1;
    sha_init(&hash); sha_byte(&hash,'a'); sha_byte(&hash,'b'); sha_byte(&hash,'c'); sha_end(&hash,digest);
    for(i=0;i<32;++i) test_hash[i]=digest[i];
    check(!memcmp(digest,expected,32));
    check(source_new(&s,"H:/SLICE.PAS"));
    check(load("begin  \r\nend. \r\n\032padding"));
    check(s.length==11&&far_get(s.page,5)=='\n'&&far_get(s.page,10)=='\n');
    for(i=0;i<128;++i) line[i]=' ';
    check(!source_import(&s,line,128,&d)&&d.code==100&&s.length==11);
    check(!source_replace(&s,0,0,line,128)&&s.length==11);
    check(source_replace(&s,5,5,(const uint8_t*)"!",1)&&s.dirty);
    check(!compile(&s,&b,&d)&&d.code==58&&pages_used()==code_required+1);
    check(load("begin writeln(32768) end."));
    check(!compile(&s,&b,&d)&&d.code==56&&pages_used()==code_required+1);
    check(load("begin writeln(-(32768)) end."));
    check(!compile(&s,&b,&d)&&d.code==56);
    test_stage=2;
    capture=open("H:/CONSOLE.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    execution("var n: integer;\nbegin n:=-32768; writeln('N=',n,',',7 div -3,',',7 mod -3) end.",1,0);
    test_stage=3;
    execution("begin\nhalt; writeln('UNREACHABLE') end.",4,0);
    test_stage=4;
    execution("begin\nwriteln(7 div 0); writeln('UNREACHABLE') end.",3,2);
    execution("var a:string[3]; b:string[8]; n:integer;\nbegin a:='abcdef'; b:=a; b:=b; n:=42; writeln(a,'/',b,'/',n); a:=''; writeln('[',a,']') end.",1,0);
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
    test_stage=5;
    /* Exhaust actual OS allocations to fail each compiler allocation stage.
     * Every failed attempt must retain the old source and its owner ledger. */
    {
        uint8_t held[32],count=0,p,available;
        while((p=page_new(PAGE_RUN))!=255) held[count++]=p;
        for(available=0;available<5;++available) {
            uint8_t owned=pages_used();
            check(!compile(&s,&b,&d)&&d.code==98);
            check(pages_used()==owned&&s.dirty);
            if(count) page_drop(held[--count]);
        }
        while(count) page_drop(held[--count]);
    }
    check(key_decode(0x98)==0); check(key_decode('k')==11);
    check(key_decode(0xfe)==0); check(key_decode(0x98)==0); check(key_decode('k')=='k');
    check(key_decode(0x96)==0); check(key_decode('a')=='A');
    key_decode(0xfe); key_decode(0x96); check(key_decode('a')=='a');
    strcpy(s.path,"H:/EDIT.PAS");
    check(load("begin\nend.\n"));
    check(source_replace(&s,5,5,(const uint8_t*)"  ",2));
    check(save_source(&s,0)&&!s.dirty);
    check(source_replace(&s,8,8,(const uint8_t*)"{second}\n",9));
    check(save_source(&s,0)&&!s.dirty);
    check(source_replace(&s,8,8,(const uint8_t*)"{third}\n",8));
    check(!save_source(&s,0)&&platform_error==15&&s.dirty&&!*output_recovery());
    check(save_source(&s,1)&&!s.dirty);
    check(load_source(&s,"H:/sub/../EDIT.PAS",&d)&&s.length==28&&!s.dirty);
    check(source_replace(&s,0,0,(const uint8_t*)" ",1));
    check(seed("H:/BLOCK","existing file"));
    check(!load_source(&s,"H:/BLOCK/never.pas",&d)&&platform_error==ERR_NOT_A_DIR&&s.length==29&&s.dirty);
    check(seed("H:/BAD.PAS","                                                                                                                                "));
    check(!load_source(&s,"H:/BAD.PAS",&d)&&d.code==100&&s.length==29&&s.dirty);
    {
        Source before;
        uint8_t old_hash[32],new_hash[32];
        memcpy(&before,&s,sizeof(s));
        source_hash(&s,old_hash);
        check(compile(&s,&b,&d));
        check(!save_source_as(&s,"H:/BLOCK/denied.pas",0)&&platform_error==ERR_NOT_A_DIR);
        check(!memcmp(&s,&before,sizeof(s))&&build_ready(&s,&b));
        check(save_source_as(&s,"h:/./RENAMED",0));
        check(!strcmp(s.path,"H:/RENAMED.PAS")&&!s.dirty&&s.revision==before.revision+1);
        check(s.page==before.page&&s.length==before.length&&s.cursor==before.cursor&&s.previous==before.previous);
        source_hash(&s,new_hash); check(!memcmp(old_hash,new_hash,32)&&!build_ready(&s,&b));
        build_close(&b);
        memcpy(&before,&s,sizeof(s));
        check(save_source_as(&s,s.path,0)&&!memcmp(&s,&before,sizeof(s)));
    }
    check(load_source(&s,"H:/NEWFILE",&d)&&!strcmp(s.path,"H:/NEWFILE.PAS")&&!s.length&&!s.dirty);
    check(pages_used()==code_required+1);
    test_stage=7;
    strcpy(s.path,"H:/NAME.PAS");
    capture=open("H:/INPUT.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    execution("program HelloName;\ntype userrec=record UserName:string[80] end;\nvar user:userrec; a:string[3]; b:string[4];\nbegin with user do begin Write('Your name: '); ReadLn(UserName); WriteLn('Hello, ',UserName,'!');\nReadLn(a,b); WriteLn(a,'/',b); ReadLn(user.UserName); WriteLn('[',UserName,']') end end.",1,0);
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
    media_tests();
    control_tests();
    ordinal_tests();
    case_tests();
    routine_tests();
    string_tests();
    string_operation_tests();
    array_tests();
    record_tests();
    set_tests();
    pointer_tests();
    heap_tests();
    memory_tests();
    label_tests();
    real_tests();
    format_tests();
    source_close(&s); check(pages_used()==code_required);
    code_hash(digest); check(!memcmp(digest,initial_code_hash,32));
    test_stage=6; test_done=255;
    test_complete(); return 0;
}
