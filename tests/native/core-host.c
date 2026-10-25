/* Host structural/failure tests for the exact C front end used on Zeal.
 * These are deliberately not counted as native execution tests. */
#include "zp.h"
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
static void checked(int value,unsigned line) { ++checks; if(!value) { fprintf(stderr,"check %u, line %u, diagnostic %u\n",checks,line,d.code); abort(); } }
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
    invalid("begin writeln(32768) end.",56);
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
    invalid("var n:integer; begin readln(n) end.",44);
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
    for(i=0;i<5;++i) {
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
    }
    source_close(&s); check(!pages_used());
    printf("PASS: %u host front-end, identity and allocation-failure checks\n",checks);
    return 0;
}
