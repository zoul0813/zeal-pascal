/* Host structural/failure tests for the exact C front end used on Zeal.
 * These are deliberately not counted as native execution tests. */
#include "compiler.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t memory[64][PAGE_SIZE],owned[64];
volatile uint32_t page_versions[64];
static int fail_after=-1;
static unsigned checks;
uint8_t page_new(uint8_t role) {
    uint8_t i;
    if(fail_after==0) return 255;
    if(fail_after>0) --fail_after;
    for(i=32;i<64;++i) if(!owned[i]) { owned[i]=role; return i; }
    return 255;
}
uint8_t page_drop(uint8_t p) { assert(p<64&&owned[p]); owned[p]=0; return 1; }
uint8_t pages_used(void) { uint8_t n=0,i; for(i=32;i<64;++i) n+=owned[i]!=0; return n; }
void page_initialized(uint8_t p,uint16_t n) { assert(p<64&&owned[p]&&n<=PAGE_SIZE); }
uint8_t far_get(uint8_t p,uint16_t at) { assert(p<64&&owned[p]&&at<PAGE_SIZE); return memory[p][at]; }
void far_set(uint8_t p,uint16_t at,uint8_t v) { assert(p<64&&owned[p]&&at<PAGE_SIZE); ++page_versions[p]; memory[p][at]=v; }
static Source s;
static Build b;
static Diagnostic d;
static void checked(int value,unsigned line) {
    ++checks;
    if(!value) {
        fprintf(stderr,"check %u, line %u, diagnostic %u at %u, emitted=%u static=%u temporaries=%u\n",
                checks,line,d.code,d.start,cc.emitted,b.globals_bytes,cc.temporaries);
        abort();
    }
}
#define check(value) checked((value),__LINE__)
static void load(const char *text) { check(source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d)); }
static void invalid(const char *text,uint8_t code) {
    load(text); check(!compile(&s,&b,&d)&&d.code==code); check(pages_used()==1);
}
static uint16_t map_word(uint16_t at) { return far_get(b.map_page,at)|((uint16_t)far_get(b.map_page,at+1)<<8); }
static void ordered_map(void) {
    uint16_t at=96+map_word(84),last=0,begin,end;
    for(;at<b.map_length;at+=16) {
        begin=map_word(at); end=map_word(at+2);
        check(begin>=last&&begin<end&&end<=b.code_bytes);
        check(map_word(at+8)<=map_word(at+12)&&map_word(at+12)<=s.length);
        last=end;
    }
}
int main(int argc,char **argv) {
    unsigned i;
    uint16_t position;
    uint8_t old;
    FILE *file;
    check(argc==2);
    check(source_new(&s,"H:/SLICE.PAS"));
    load("begin  \r\nend. \r\n\032padding"); check(s.length==11);
    check(source_replace(&s,0,0,NULL,0)&&!s.dirty);
    {
        const uint8_t embedded[]="begin write('a\0b') end.";
        check(source_import(&s,embedded,sizeof(embedded)-1,&d));
        check(compile(&s,&b,&d)); build_close(&b);
    }
    invalid("var r:real;begin r:=true end.",44);
    invalid("var n:integer;begin n:=1/2 end.",44);
    invalid("var r:real;begin r:=1e38 end.",57);
    invalid("var r:real;begin r:=1e-39 end.",57);
    invalid("var r:real;begin r:=1e end.",57);
    invalid("var r:real;begin r:=1e001 end.",1);
    invalid("var r:real;begin r:=1.0e38 end.",57);
    invalid("type t=1.0..2.0;begin end.",57);
    invalid("begin writeln(real(1))end.",30);
    invalid("begin writeln(integer(1.5))end.",31);
    invalid("begin writeln(ord(1.0))end.",31);
    invalid("begin writeln(sqrt(true))end.",23);
    invalid("begin writeln(sqrt('x'))end.",23);
    invalid("begin writeln(sqrt('xx'))end.",23);
    invalid("begin writeln(ln(true))end.",23);
    invalid("begin writeln(exp('x'))end.",23);
    invalid("begin writeln(sin(true))end.",23);
    invalid("begin writeln(cos('x'))end.",23);
    invalid("begin writeln(arctan('xx'))end.",23);
    invalid("begin writeln(random())end.",41);
    invalid("begin writeln(random(1.5))end.",23);
    invalid("begin writeln(random(true))end.",23);
    invalid("begin writeln(random('A'))end.",23);
    invalid("type t=(a,b);begin writeln(random(a))end.",23);
    invalid("begin writeln(random(1,2))end.",5);
    invalid("begin randomize()end.",1);
    load("var a:array[0..1]of integer;begin writeln(sizeof(a[random(2)]))end.");
    check(compile(&s,&b,&d)); check(!cc.random_entry&&!cc.random_state);
    build_close(&b);
    invalid("begin writeln(succ(1.0))end.",31);
    invalid("begin writeln(1.0 div 2)end.",47);
    invalid("begin writeln(1.0 and 2)end.",47);
    invalid("var r:real;begin for r:=1 to 2 do end.",30);
    invalid("type a=array[real]of byte;begin end.",30);
    invalid("type a=set of real;begin end.",30);
    invalid("begin writeln(1:5:2)end.",5);
    invalid("begin writeln('x':5:2)end.",5);
    invalid("begin writeln(true:5:2)end.",5);
    invalid("begin writeln('xx':5:2)end.",5);
    invalid("begin writeln(1:true)end.",23);
    invalid("begin writeln(1.0:5:true)end.",23);
    invalid("var s:string[80];begin str(true,s)end.",26);
    invalid("var s:string[80];begin str('x',s)end.",26);
    invalid("var s:string[80];begin str('xx',s)end.",26);
    invalid("var s:string[80];begin str(1:5:2,s)end.",3);
    invalid("var c:char;begin str(1,c)end.",34);
    invalid("begin str(1,'x')end.",41);
    invalid("type t=(a,b);begin writeln(1:a)end.",23);
    invalid("var b:byte;c:integer;begin val('1',b,c)end.",27);
    invalid("type s=1..5;var n:s;c:integer;begin val('1',n,c)end.",27);
    invalid("var b:char;c:integer;begin val('1',b,c)end.",27);
    invalid("var n:integer;c:byte;begin val('1',n,c)end.",24);
    invalid("type s=0..5;var n:integer;c:s;begin val('1',n,c)end.",24);
    invalid("var n,c:real;begin val('1',n,c)end.",24);
    invalid("var c:integer;begin val('1',1,c)end.",41);
    invalid("var n:integer;begin val('1',n,0)end.",41);
    invalid("var n,c:integer;begin val(1,n,c)end.",33);
    invalid("begin goto missing end.",40);
    invalid("label l;begin goto l end.",40);
    invalid("label l,l;begin end.",43);
    invalid("label l;begin l:;l:end.",43);
    invalid("label l;var l:integer;begin end.",43);
    invalid("begin missing:end.",41);
    invalid("label l;var r:record l:integer end;begin with r do begin l:l:=1 end end.",7);
    invalid("label l;procedure p;begin goto l end;begin l:end.",72);
    invalid("label l;procedure p;begin l:end;begin l:end.",72);
    invalid("label l;var i:integer;begin goto l;for i:=1 to 1 do l:halt end.",71);
    invalid("label l;var i:integer;begin if false then for i:=1 to 1 do l:halt;goto l end.",71);
    invalid("label 000;begin goto 0;000:end.",40);
    invalid("label $1;begin end.",58);
    invalid("label 1;begin goto $1;1:end.",40);
    invalid("label 1;begin $1:end.",41);
    invalid("begin writeln(12345678901234567890)end.",56);
    invalid("const x=-12345678901234567890;begin end.",56);
    load("label unused,01,1;begin goto 01;01:goto 1;1:end.");
    check(compile(&s,&b,&d)); ordered_map(); build_close(&b);
    invalid("var a,b:byte absolute $8000;begin end.",75);
    invalid("var b:byte absolute 'A';begin end.",22);
    invalid("var b:byte absolute true;begin end.",22);
    invalid("var b:byte absolute missing;begin end.",41);
    invalid("var b:byte absolute b;begin end.",41);
    invalid("var a:array[1..2]of byte;b:byte absolute a[2];begin end.",1);
    invalid("var n:integer;b:byte absolute n+1;begin end.",1);
    invalid("var n:integer;b:byte absolute addr(n);begin end.",41);
    invalid("function f:byte;var b:byte absolute f;begin end;begin end.",41);
    invalid("var n:integer;begin mem['A']:=0 end.",23);
    invalid("var n:integer;begin mem[addr(n)]:='A' end.",44);
    invalid("var n:integer;begin mem[addr(n)]:=true end.",44);
    invalid("begin port['A']:=0 end.",23);
    invalid("begin port[0]:='A' end.",23);
    invalid("begin writeln(addr(port[0]))end.",41);
    invalid("begin writeln(sizeof(port[0]))end.",41);
    invalid("begin writeln(sizeof(mem))end.",8);
    invalid("procedure p(var b:byte);begin end;begin p(port[0])end.",41);
    invalid("var a:byte;begin fillchar(a,1,ptr(0))end.",31);
    invalid("var a:byte;begin fillchar(a,'A',0)end.",23);
    invalid("var a,b:byte;begin move(a,b,true)end.",23);
    invalid("var a:byte;begin move('AB',a,1)end.",41);
    invalid("function f:byte;begin fillchar(f,1,0)end;begin end.",41);
    invalid("var p:integer;begin new(p)end.",44);
    invalid("begin new(nil)end.",44);
    invalid("begin release(0)end.",44);
    invalid("type p=^integer;var x:p;begin getmem(x,true)end.",23);
    invalid("begin stackptr:=true end.",23);
    invalid("begin writeln(addr(stackptr))end.",44);
    invalid("begin writeln(memavail())end.",5);
    invalid("type n=record p:^n end;begin end.",42);
    invalid("type n=record p:^later end;later=integer;begin end.",42);
    load("var p:^integer;integer:char;begin integer:='A';writeln(sizeof(p^))end.");
    check(compile(&s,&b,&d)); build_close(&b);
    load("type n=integer;r=record p:^n end;var x:r;begin x.p:=nil end.");
    check(compile(&s,&b,&d)); build_close(&b);
    invalid("type p=^missing;begin end.",42);
    invalid("type p=^later;var later:integer;begin end.",42);
    invalid("type p=^integer;const x:p=nil;begin end.",61);
    invalid("const x=nil;begin end.",1);
    invalid("type p=^integer;var x:p;begin x:=p(0)end.",30);
    invalid("type p=^integer;var x:p;begin x:=0 end.",44);
    invalid("type p=^integer;q=^char;var x:p;y:q;begin x:=y end.",44);
    invalid("type p=^integer;var x:p;begin writeln(x<nil)end.",47);
    invalid("type p=^integer;var x:p;begin x:=x+1 end.",47);
    invalid("type p=^integer;function f:p;begin f^:=1 end;begin end.",7);
    invalid("type p=^integer;function f:p;begin f:=nil end;begin writeln(f^)end.",66);
    invalid("type p=^integer;q=^char;var x:q;procedure f(var a:p);begin end;begin f(x)end.",44);
    invalid("begin writeln(succ(nil))end.",31);
    invalid("begin writeln(pred(nil))end.",31);
    invalid("begin writeln(succ(ptr(0)))end.",31);
    invalid("begin writeln(pred(ptr(0)))end.",31);
    invalid("begin writeln(succ([]))end.",31);
    invalid("begin writeln(pred([1]))end.",31);
    invalid("begin writeln(32768) end.",56);
    invalid("var s:string[3]; begin s[true]:='x' end.",23);
    invalid("var s:string[3]; begin s[1][1]:='x' end.",7);
    invalid("var n:integer; begin n[1]:=2 end.",7);
    invalid("var s:string[3]; begin for s[1]:='a' to 'z' do end.",30);
    invalid("type t=string[3]; function f:t; begin f[1]:='x' end; begin end.",7);
    invalid("type t=string[3]; function f:t; begin f:='x' end; begin writeln(f[1]) end.",5);
    invalid("type t=string[3]; procedure p(var s:t); begin end; function f:t; begin p(f) end; begin end.",41);
    invalid("begin writeln(concat()) end.",41);
    invalid("begin writeln(copy(3,1,2)) end.",33);
    invalid("begin writeln(copy('abc',true,2)) end.",23);
    invalid("var s:string[3]; begin delete(s[1],1,2) end.",34);
    invalid("var s:string[3]; begin insert('x',s[1],1) end.",34);
    invalid("var s:string[3]; begin s[1:='x' end.",9);
    invalid("var a:array[boolean] of byte; begin a[1]:=0 end.",44);
    invalid("var a:array[1..3] of byte; begin a[true]:=0 end.",44);
    invalid("type t=array[1..2] of byte; function f:t; begin end; begin end.",48);
    invalid("type a=array[1..2] of byte; b=array[1..2] of byte; var x:a; y:b; begin x:=y end.",44);
    invalid("var a,b:array[1..2] of byte; begin writeln(a=b) end.",62);
    invalid("var a:array[1..256] of char; begin writeln(length(a)) end.",62);
    invalid("var a:array[boolean] of char; begin writeln(length(a)) end.",62);
    invalid("var a:array['a'..'c'] of char; begin writeln(length(a)) end.",62);
    invalid("var a:array[1..3] of char; s:string[3]; begin a:=s end.",44);
    invalid("var a:array[1..3] of char; begin a:='ab' end.",60);
    invalid("var a:array[1..1] of char; begin a:='x' end.",60);
    invalid("var a:array[1..3] of char; begin a:='abc'+'d' end.",1);
    invalid("var a:array[1..3] of char; begin a:=('abc') end.",41);
    invalid("type t=array[integer] of byte; begin end.",98);
    invalid("type t=array[string[3]] of byte; begin end.",30);
    invalid("const a:array[1..2] of integer=(1,2,3); begin end.",5);
    invalid("const a:array[1..3] of byte=(1,256,3); begin end.",44);
    invalid("type r=record a:integer end; var x:r; begin x.z:=1 end.",41);
    invalid("type r=record a,a:integer end; begin end.",43);
    invalid("type r=record case boolean of false:(n:integer);true:(n:char) end;begin end.",43);
    invalid("type a=record n:integer end; b=record n:integer end;var x:a;y:b;begin x:=y end.",44);
    invalid("type r=record n:integer end;function f:r;begin end;begin end.",48);
    invalid("type r=record n:integer end;var x,y:r;begin writeln(x=y) end.",62);
    invalid("type r=record a,b:integer end;const x:r=(b:2;a:1);begin end.",69);
    invalid("type r=record a,b,c:byte end;const x:r=(a:1;c:3);begin end.",69);
    invalid("type r=record case tag:boolean of false:(n:integer) end;const x:r=(n:1);begin end.",69);
    invalid("type r=record case missing of 0:(n:integer) end;begin end.",2);
    invalid("type r=record end;const x:r=();begin end.",41);
    invalid("var n:integer;begin with n do end.",29);
    invalid("{$W0}type r=record n:integer end;var x:r;begin with x do end.",97);
    invalid("{$W1}type r=record n:integer end;var x:r;begin with x,x do end.",97);
    invalid("type r=record n:integer end;var x:r;begin with x,x,x,x,x do end.",97);
    invalid("type r=record n:integer end;var x:r;begin {$W9}with x,x,x,x,x do end.",97);
    invalid("type r=record n:integer end;var x:r;begin with x do for n:=1 to 3 do end.",41);
    invalid("type r=record n:integer end;var x:r;begin for x.n:=1 to 3 do end.",30);
    invalid("type t=set of integer;begin end.",70);
    invalid("type t=set of -1..3;begin end.",70);
    invalid("type t=set of 0..256;begin end.",70);
    invalid("type t=set of byte;function f:t;begin end;begin end.",48);
    invalid("begin writeln(1 in [1,true])end.",44);
    invalid("begin writeln([1]=[true])end.",44);
    invalid("var a,b:set of byte;begin writeln(a<b)end.",47);
    invalid("type t=set of 3..5;const x:t=[0];begin end.",45);
    invalid("type t=set of byte;const x:t=[true];begin end.",44);
    invalid("const x=[1];begin end.",1);
    invalid("var s:set of byte;begin writeln(s)end.",66);
    invalid("type r=record n:integer end;var x:r;begin writeln(x)end.",62);
    invalid("type t=set of 3..5;u=set of 3..5;var x:t;procedure p(var v:u);begin end;begin p(x)end.",44);
    invalid("begin writeln(+32768) end.",56);
    invalid("begin writeln(-32769) end.",56);
    invalid("begin writeln(-(32768)) end.",56);
    invalid("begin writeln('unterminated) end.",55);
    invalid("begin {$Z+} end.",93);
    invalid("begin nope:=1 end.",41);
    invalid("var n,n: integer; begin end.",43);
    invalid("begin writeln(1) writeln(2) end.",1);
    invalid("program begin; begin end.",53);
    invalid("begin {unterminated",91);
    invalid("begin ! end.",58);
    invalid("var s:string[0]; begin end.",49);
    invalid("var s:string[256]; begin end.",49);
    invalid("var s:string; begin end.",8);
    invalid("var s:string[80; begin end.",9);
    invalid("var s:string[8]; begin s:=42 end.",44);
    invalid("var n:integer; begin n:='text' end.",44);
    invalid("var s:string[8]; begin writeln(s*2) end.",47);
    invalid("var s:string[8]; begin writeln(-s) end.",47);
    invalid("var n:boolean; begin readln(n) end.",66);
    invalid("type t=(a,b);var n:t;begin read(n)end.",66);
    invalid("var a:array[1..3]of char;begin readln(a)end.",66);
    invalid("begin readln(1)end.",41);
    invalid("begin readln()end.",41);
    invalid("begin read()end.",41);
    invalid("begin {$B?}end.",93);
    invalid("begin readln(missing) end.",41);
    invalid("begin if 1 then end.",20);
    invalid("begin while 'a' do end.",20);
    invalid("begin repeat until 1 end.",20);
    invalid("begin if true writeln(1) end.",17);
    invalid("begin while true writeln(1) end.",13);
    invalid("begin if true then writeln(1); else writeln(2) end.",1);
    invalid("var b:boolean; begin b:=1 end.",44);
    invalid("begin writeln(true+false) end.",47);
    invalid("begin writeln(1 and true) end.",47);
    invalid("begin writeln(not 'x') end.",47);
    invalid("begin writeln(1='x') end.",47);
    invalid("var i:integer; begin for i:=1 do end.",18);
    invalid("var i:integer; begin for i:=1 to 3 i:=2 end.",13);
    invalid("var i:integer; begin for i:=1 to 3 do i:=2 end.",44);
    invalid("var i:integer; begin for i:=1 to 3 do for i:=1 to 2 do end.",44);
    invalid("var i:integer; begin for i:=true to false do end.",44);
    invalid("const x=1+2; begin end.",1);
    invalid("const x=x; begin end.",41);
    invalid("const x=1; begin x:=2 end.",44);
    invalid("type t=integer; begin t:=2 end.",44);
    invalid("type t=5..1; begin end.",52);
    invalid("type t=1..true; begin end.",51);
    invalid("type t='ab'..'cd'; begin end.",51);
    invalid("type a=(red,green); b=(blue,yellow); var x:a; begin x:=blue end.",44);
    invalid("type a=(red,green); b=(blue,yellow); begin writeln(red=blue) end.",47);
    invalid("type a=(red,green); begin writeln(red) end.",44);
    invalid("const n:byte=256; begin end.",44);
    invalid("var n:integer; const x=n; begin end.",41);
    invalid("begin writeln(ord([])) end.",31);
    invalid("begin writeln(chr(true)) end.",23);
    invalid("begin writeln(#256) end.",56);
    invalid("begin writeln(#$100) end.",56);
    invalid("begin writeln($00000) end.",56);
    invalid("begin case 'text' of end end.",41);
    invalid("begin case 1 of end end.",41);
    invalid("begin case 1 of else end end.",41);
    invalid("begin case 1 end end.",15);
    invalid("begin case 1 of 1 end end.",3);
    invalid("begin case 1 of 'a': end end.",46);
    invalid("begin case 'a' of 1: end end.",46);
    invalid("begin case true of 1: end end.",46);
    invalid("begin case 1 of missing: end end.",41);
    invalid("var n:integer; begin case 1 of n: end end.",41);
    invalid("type t=(red,green); u=(blue,white); begin case red of blue: end end.",46);
    invalid("begin case 1 of 0..true: end end.",46);
    invalid("begin case 1 of 1:;",14);
    invalid("procedure p; forward; begin end.",73);
    invalid("procedure p; begin end; procedure p; begin end; begin end.",43);
    invalid("procedure p(a:integer); begin end; begin p end.",4);
    invalid("procedure p(a:integer); begin end; begin p(true) end.",44);
    invalid("procedure p(var a:integer); begin end; begin p(1) end.",44);
    invalid("var b:byte; procedure p(var a:integer); begin end; begin p(b) end.",44);
    invalid("procedure p(var a); begin a:=1 end; begin end.",44);
    invalid("procedure p(var a); begin writeln(a) end; begin end.",44);
    invalid("procedure p; var hidden:integer; begin end; begin hidden:=1 end.",41);
    invalid("begin writeln(length(1)) end.",44);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin writeln(f-1) end.",47);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin writeln(1+f) end.",47);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin writeln(f+true) end.",47);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin if f then end.",20);
    invalid("type text=string[3]; var c:char; function f:text; begin f:='a' end; begin for c:=f to 'z' do end.",44);
    invalid("type text=string[3]; var c:char; function f:text; begin f:='a' end; begin for c:='a' to f do end.",44);
    invalid("type text=string[3]; var s:string[4]; procedure p(var a:text); begin end; begin p(s) end.",44);
    load("type text=string[3]; function f:text; begin f:='abc' end; begin writeln(f) end.");
    check(compile(&s,&b,&d)); ordered_map(); build_close(&b);
    load("const x=-32768; y=x; z=-y; type octet=byte; text=string[3]; var n:octet; s:text; begin n:=257; s:='x'; writeln(x,y,z,n,s) end.");
    check(compile(&s,&b,&d)&&b.globals_bytes==5); build_close(&b);
    load("var b:boolean; n:integer; begin b:=not false; n:=1234; writeln(b,n) end.");
    check(compile(&s,&b,&d)&&b.globals_bytes==3); ordered_map(); build_close(&b);
    load("var i,n:integer; begin if true then if false then n:=1 else n:=2;\n"
         "for i:=1 to 3 do begin while false do n:=9; repeat n:=n-1 until n=0 end end.");
    check(compile(&s,&b,&d)); ordered_map(); build_close(&b);
    load("var a,b:string[80]; n:integer; c:string[255]; begin a:='Hello'; b:=a; c:=''; writeln(b,n,c); readln(a) end.");
    check(compile(&s,&b,&d)); check(b.globals_bytes==420&&b.code_origin==GLOBAL_BASE+420);
    build_close(&b);
    load("var writeln: integer; begin writeln:=1 end.");
    check(compile(&s,&b,&d)); build_close(&b);
    load("program Slice;\nvar n: integer;\nbegin n:=-32768; writeln('N=',n,',',7 div -3,',',7 mod -3) end.");
    for(i=0;i<6;++i) {
        fail_after=(int)i;
        check(!compile(&s,&b,&d)&&d.code==98); check(pages_used()==1);
    }
    fail_after=-1;
    check(compile(&s,&b,&d)); check(build_check(&s,&b)); check(build_ready(&s,&b));
    old=far_get(b.map_page,b.map_length-1);
    far_set(b.map_page,b.map_length-1,old^1);
    check(!build_check(&s,&b));
    check(!build_ready(&s,&b));
    far_set(b.map_page,b.map_length-1,old); check(build_check(&s,&b));
    old=far_get(b.pages[1],0);
    far_set(b.pages[1],0,old^1); check(!build_check(&s,&b)); check(!build_ready(&s,&b));
    far_set(b.pages[1],0,old); check(build_check(&s,&b));
    check(!build_locate(&s,&b,b.code_bytes,&position));
    check(build_locate(&s,&b,0,&position));
    check(position==(uint16_t)(strstr("program Slice;\nvar n: integer;\nbegin n:=-32768;", "n:=-")-"program Slice;\nvar n: integer;\nbegin n:=-32768;"));
    file=fopen(argv[1],"wb"); check(file!=NULL);
    for(i=0;i<b.length;++i) fputc(image_get(&b,(uint16_t)i),file);
    check(!fclose(file));
    check(source_replace(&s,s.length,s.length,(const uint8_t*)" ",1));
    check(!build_check(&s,&b)); build_close(&b);
    load("begin\nwriteln(7 div 0)\nend.");
    check(source_replace(&s,5,5,(const uint8_t*)"   ",3));
    check(compile(&s,&b,&d));
    check(build_locate(&s,&b,0,&position)&&position==9);
    build_close(&b);
    {
        static uint8_t text[SOURCE_LIMIT];
        uint16_t n;
        file=fopen("tests/native/control.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        check(b.globals_bytes==29); ordered_map(); build_close(&b);
        file=fopen("tests/native/ordinals.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        check(b.globals_bytes==19); ordered_map(); build_close(&b);
        file=fopen("tests/native/cases.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/routines.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/stringfn.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/strlife.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/strbound.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        {
            const char *fixtures[]={"strindex","strops","strop255","arrays","arrayarg","chararr","records","variants","withrec","zerorec","sets","setshape","setargs","setabi","pointers","ptrargs","ordchar","heapmark","heapfree","pointerabi","absolute","absframe","rawmemory","nativeio","gotos","reals","realabi","formatout","strformat","valparse","readinput","endchar","sqrt","sqrtshadow","logexp","trig","trigshadow","random","randshadow","randomseed"};
            unsigned fixture;
            char path[80];
            for(fixture=0;fixture<sizeof(fixtures)/sizeof(fixtures[0]);++fixture) {
                snprintf(path,sizeof(path),"tests/native/%s.pas",fixtures[fixture]);
                file=fopen(path,"rb"); check(file!=NULL);
                n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
                check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
                if(!strcmp(fixtures[fixture],"absolute")) check(b.globals_bytes==8);
                if(!strcmp(fixtures[fixture],"realabi")) check(b.globals_bytes==0);
                ordered_map(); build_close(&b);
            }
        }
        {
            const char *names[]={"stringabi","setabi","pointerabi"};
            unsigned abi;
            char path[80];
            for(abi=0;abi<sizeof(names)/sizeof(names[0]);++abi) {
                snprintf(path,sizeof(path),"tests/native/%s.pas",names[abi]);
                file=fopen(path,"rb"); check(file!=NULL);
                n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
                check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
                check(b.globals_bytes==0&&b.code_origin==GLOBAL_BASE);
                snprintf(path,sizeof(path),"build/%s.zpl",names[abi]);
                file=fopen(path,"rb"); check(file!=NULL);
                n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
                check(n==image_get(&b,0x4003));
                for(i=0;i<n;++i) check(image_get(&b,0x4004+(uint16_t)i)==text[i]);
                ordered_map(); build_close(&b);
            }
        }
        for(i=256;i<=257;++i) {
            unsigned member;
            n=(uint16_t)snprintf((char*)text,sizeof(text),"type t=(");
            for(member=0;member<i;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"v%u%s%s",member,member+1<i?",":"",member%10==9?"\n":"");
            n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"); var x:t; begin x:=v%u; writeln(ord(x),sizeof(t)) end.",i-1);
            check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
            check(b.globals_bytes==(i==256?1:2)); build_close(&b);
        }
        {
            unsigned member;
            n=(uint16_t)snprintf((char*)text,sizeof(text),"type t=(");
            for(member=0;member<513;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"v%u%s%s",member,member<512?",":"",member%10==9?"\n":"");
            snprintf((char*)text+n,sizeof(text)-n,"); begin end.");
            invalid((char*)text,98);
            n=(uint16_t)snprintf((char*)text,sizeof(text),"type\n");
            for(member=0;member<60;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"t%u=string[1];\n",member);
            snprintf((char*)text+n,sizeof(text)-n,"begin end.");
            invalid((char*)text,98);
        }
        {
            char left[128],right[128];
            memset(left,'a',126); left[126]='x'; left[127]=0;
            memcpy(right,left,128); right[126]='y';
            n=(uint16_t)snprintf((char*)text,sizeof(text),"var\n%s\n,\n%s\n:byte;\nbegin\n%s\n:=1;\n%s\n:=2 end.",left,right,left,right);
            check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
            check(b.globals_bytes==2); build_close(&b);
        }
        {
            unsigned arm;
            n=(uint16_t)snprintf((char*)text,sizeof(text),"begin\n");
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"case 1 of 1:\n");
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"end\n");
            snprintf((char*)text+n,sizeof(text)-n,"end.");
            invalid((char*)text,98);
            n=(uint16_t)snprintf((char*)text,sizeof(text),"begin case 1 of\n");
            for(arm=0;arm<600;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"%u:;\n",arm);
            snprintf((char*)text+n,sizeof(text)-n,"end end.");
            invalid((char*)text,99);
            n=0;
            for(arm=0;arm<256;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"procedure p%u; forward;\n",arm);
            snprintf((char*)text+n,sizeof(text)-n,"begin end.");
            invalid((char*)text,98);
            n=0;
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"procedure p%u;\n",arm);
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"begin end;\n");
            snprintf((char*)text+n,sizeof(text)-n,"begin end.");
            invalid((char*)text,98);
        }
    }
    source_close(&s); check(!pages_used());
    printf("PASS: %u host front-end, identity and allocation-failure checks\n",checks);
    return 0;
}
