#include "harness.h"
#include "compiler/internal.h"

/* Deliberate proof-only address interval, not an allocator or public ABI.
 * Production runtime/linker and ordinary global placement stay unchanged. */
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_done;
Source s;Build b;Diagnostic d;
void test_complete(void);

static uint8_t physical_cell(uint16_t address) __naked {
    __asm
        ld a,i
        push af
        di
        ld a,#0x40
        in a,(0xf1)
        push af
        ld a,#0x40
        out (0xf1),a
        ld b,(hl)
        pop af
        out (0xf1),a
        pop af
        jp po,001$
        ei
001$:
        ld a,b
        ret
    __endasm;
}
static void rejected_transfer(const char *text) {
    check(load(text));
    check(!compile(&s,&b,&d)&&d.code==COMPILE_PLATFORM_FAILURE);
    build_close(&b);check(pages_used()==code_required+1);
}

/* Shared rejection cases also get a RAM-only bytewise destination oracle.
 * COM exit/error behavior is checked by the ordinary harness for these sources. */
static const char typed_min[]=
    "var pad:array[1..512]of byte;f:file of integer;n:integer absolute $5fff;\n"
    "begin n:=$a5a5;assign(f,'H:/READTYPE.DAT');reset(f);read(f,n)end.";
static const char typed_cross[]=
    "var pad:array[1..512]of byte;f:file of integer;n:integer absolute $7fff;\n"
    "begin n:=$a5a5;assign(f,'H:/READTYPE.DAT');reset(f);read(f,n)end.";
static const char block_min[]=
    "var pad:array[1..512]of byte;f:file;low:array[0..127]of byte absolute $5fff;\n"
    "actual:integer;begin fillchar(low,128,$a5);assign(f,'H:/READBLOCK.DAT');\n"
    "reset(f);blockread(f,low,1,actual)end.";
static const char block_cross[]=
    "var pad:array[1..512]of byte;f:file;low:array[0..127]of byte absolute $7fc0;\n"
    "actual:integer;begin fillchar(low,128,$a5);assign(f,'H:/READBLOCK.DAT');\n"
    "reset(f);blockread(f,low,1,actual)end.";
static void rejected_read_guard(const char *text,uint16_t address,uint16_t count) {
    uint16_t i;
    check(load(text));check(compile(&s,&b,&d));
    if(b.valid) {
        check(run_build(&s,&b)&&ram_result[26]==3&&ram_result[28]==0xff);
        for(i=0;i<count;++i)check(image_get(&b,address-0x4000u+i)==0xa5);
        check(build_check(&s,&b));
    }
    build_close(&b);check(pages_used()==code_required+1);
}

int app_main(void) {
    int8_t capture;
    if(!console_open())return 1;
    check(source_new(&s,"H:/SLICE.PAS"));
    check(cc_runtime_size()<=0x2000u);
    check(code_capture(0));
    capture=open("H:/COMPACT.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=53; /* Existing harness runs each fixture in RAM and saved COM. */
    execution("begin end.",1,0);
    execution("var n:integer absolute $6000;c:integer;begin \n"
        "val('-32768',n,c);if(c<>0)or(n<>-32768)then halt;\n"
        "val('$FFFF',n,c);if(c<>0)or(n<>-1)then halt;\n"
        "n:=123;val('32768',n,c);if(c<>6)or(n<>123)then halt;\n"
        "val('12X',n,c);if(c<>3)or(n<>123)then halt;\n"
        "val('',n,c);if(c<>0)or(n<>123)then halt;\n"
        "writeln('LOW-VAL-OK')end.",1,0);
    execution("var low:array[0..8191]of byte absolute $6000;i:integer;begin \n"
        "for i:=0 to 8191 do low[i]:=i mod 251;\n"
        "clrscr;gotoxy(7,9);textcolor(14);\n"
        "write(Con,'Q');if(WhereX<>8)or(WhereY<>9)then halt;\n"
        "for i:=0 to 8191 do if low[i]<>i mod 251 then halt;\n"
        "writeln('LOW-CRT-OK')end.",1,0);
    check(physical_cell(0x4286)=='Q'&&physical_cell(0x5286)==14&&physical_cell(0x4000)==0);
    execution("var f:file;high:array[0..127]of byte;actual,i:integer;begin \n"
        "for i:=0 to 127 do high[i]:=i;assign(f,'H:/HIGH.DAT');rewrite(f);\n"
        "blockwrite(f,high,1,actual);if actual<>1 then halt;close(f);\n"
        "fillchar(high,128,0);reset(f);blockread(f,high,1,actual);close(f);\n"
        "if actual<>1 then halt;for i:=0 to 127 do if high[i]<>i then halt;\n"
        "writeln('HIGH-FILE-OK')end.",1,0);
    execution("var n:integer absolute $6000;m:integer;begin readln(n,m);\n"
        "if(n<>-32768)or(m<>32767)then halt;writeln('LOW-INPUT-OK')end.",1,0);
    /* Whole low interval, including the last byte, traverses module staging. */
    execution("var high:byte;f:file;low:array[0..8191]of byte absolute $6000;\n"
        "guard:byte absolute $5fff;i,actual:integer;begin high:=$a7;guard:=$a6;\n"
        "for i:=0 to 8191 do low[i]:=i mod 251;assign(f,'H:/LOW.DAT');rewrite(f);\n"
        "blockwrite(f,low,64,actual);if actual<>64 then halt;close(f);\n"
        "fillchar(low,8192,0);reset(f);blockread(f,low,64,actual);close(f);\n"
        "if actual<>64 then halt;for i:=0 to 8191 do if low[i]<>i mod 251 then halt;\n"
        "if(guard<>$a6)or(high<>$a7)then halt;writeln('LOW-FILE-OK')end.",1,0);
    execution("type data=array[1..300]of byte;var f:file of data;\n"
        "low:data absolute $6000;i:integer;begin for i:=1 to 300 do low[i]:=i mod 251;\n"
        "assign(f,'H:/LOWTYPE.DAT');rewrite(f);write(f,low);close(f);fillchar(low,300,0);\n"
        "reset(f);read(f,low);close(f);for i:=1 to 300 do if low[i]<>i mod 251 then halt;\n"
        "writeln('LOW-TYPED-OK')end.",1,0);
    execution("var f:file of byte;last:byte absolute $7fff;begin last:=231;\n"
        "assign(f,'H:/LOWBYTE.DAT');rewrite(f);write(f,last);close(f);last:=0;\n"
        "reset(f);read(f,last);close(f);if last<>231 then halt;writeln('LOW-EDGE-OK')end.",1,0);
    execution("var f:file;low:array[0..255]of byte absolute $6000;i,actual:integer;begin\n"
        "fillchar(low,256,$cc);assign(f,'H:/LOWTAIL.DAT');reset(f);\n"
        "blockread(f,low,2,actual);close(f);if actual<>1 then halt;\n"
        "for i:=0 to 127 do if low[i]<>i then halt;if low[128]<>126 then halt;\n"
        "for i:=129 to 255 do if low[i]<>$cc then halt;writeln('LOW-PARTIAL-OK')end.",1,0);
    execution("var f:file;low:byte absolute $6000;actual:integer;begin low:=77;\n"
        "assign(f,'H:/LOWZERO.DAT');rewrite(f);blockwrite(f,low,0,actual);close(f);\n"
        "if(actual<>0)or(low<>77)then halt;writeln('LOW-ZERO-OK')end.",1,0);
    checked_error("var f:file;low:array[0..127]of byte absolute $5fff;actual:integer;\n"
        "begin assign(f,'H:/BADMIN.DAT');rewrite(f);\n"
        "{$I-}blockwrite(f,low,1,actual)end.",0xff,"blockwrite");
    checked_error("var f:file;low:array[0..127]of byte absolute $7fc0;actual:integer;\n"
        "begin assign(f,'H:/BADCROSS.DAT');rewrite(f);\n"
        "{$I-}blockwrite(f,low,1,actual)end.",0xff,"blockwrite");
    checked_error("var f:file absolute $6000;begin\nassign(f,'H:/BADFIB.DAT')end.",0xff,"assign");
    /* Assign already snapshots its path on the high caller stack. Reuse it. */
    execution("var f:file;s:string[20] absolute $6000;begin s:='H:/LOWPATH.DAT';\n"
        "assign(f,s);rewrite(f);close(f);writeln('LOW-PATH-OK')end.",1,0);
    checked_error("var f:file of integer;n:integer absolute $5fff;begin\n"
        "assign(f,'H:/TWMIN.DAT');rewrite(f);write(f,n)end.",0xff,"write(f,n)");
    checked_error("var f:file of integer;n:integer absolute $7fff;begin\n"
        "assign(f,'H:/TWCROSS.DAT');rewrite(f);write(f,n)end.",0xff,"write(f,n)");
    checked_error(typed_min,0xff,"read(f");checked_error(typed_cross,0xff,"read(f");
    checked_error(block_min,0xff,"blockread");checked_error(block_cross,0xff,"blockread");
    rejected_read_guard(typed_min,0x5fff,2);rejected_read_guard(typed_cross,0x7fff,2);
    rejected_read_guard(block_min,0x5fff,128);rejected_read_guard(block_cross,0x7fc0,128);
    checked_error("var f:file;low:array[0..8191]of byte absolute $6000;actual:integer;\n"
        "begin assign(f,'H:/BAD65.DAT');rewrite(f);blockwrite(f,low,65,actual)end.",0xff,"blockwrite");
    checked_error("var f:file;low:array[0..8191]of byte absolute $6000;actual:integer;\n"
        "begin assign(f,'H:/BAD511.DAT');rewrite(f);blockwrite(f,low,511,actual)end.",0xff,"blockwrite");
    /* Force live FIB, canary and code into the page2 alias offsets. */
    execution("var pad:array[0..8191]of byte;f:file;high:array[0..255]of byte;\n"
        "low:array[0..511]of byte absolute $6000;i,actual:integer;begin\n"
        "if(addr(f)<>$a000)or(addr(high)<>$a0b0)then halt;fillchar(high,256,$5a);\n"
        "for i:=0 to 511 do low[i]:=i mod 251;assign(f,'H:/ALIAS.DAT');rewrite(f);\n"
        "blockwrite(f,low,4,actual);if(actual<>4)or(filepos(f)<>4)or(filesize(f)<>4)then halt;\n"
        "close(f);fillchar(low,512,0);reset(f);blockread(f,low,4,actual);\n"
        "if(actual<>4)or(filepos(f)<>4)or(filesize(f)<>4)then halt;close(f);\n"
        "for i:=0 to 511 do if low[i]<>i mod 251 then halt;\n"
        "for i:=0 to 255 do if high[i]<>$5a then halt;writeln('HIGH-ALIAS-OK')end.",1,0);
    rejected_transfer("var f:file;begin chain(f)end.");
    rejected_transfer("var f:file;begin execute(f)end.");
    {
        CompileOptions options;
        memset(&options,0,sizeof(options));options.kind=COMPILE_CHAIN;
        check(load("begin end."));
        check(!compile_project(&s,&b,&d,0,&options)&&d.code==COMPILE_PLATFORM_FAILURE);
        build_close(&b);check(pages_used()==code_required+1);
    }
    /* Unsupported operations must fail through every assembly adapter path. */
    checked_error("var x:real;begin x:=1.5;\nx:=x+2.5 end.",0xff,"x:=x+");
    checked_error("var x:real;begin x:=1.5;\nwriteln(x)end.",0xff,"writeln");
    checked_error("var x:real;c:integer;begin\nval('1.5',x,c)end.",0xff,"val");
    checked_error("var x:real;begin x:=1.5;\nif x<2.5 then halt end.",0xff,"if");
    checked_error("var x:real;n:integer;begin n:=3;\nx:=n end.",0xff,"x:=n");
    checked_error("var x:real;n:integer;begin x:=1.5;\nn:=trunc(x)end.",0xff,"n:=trunc");
    checked_error("var x:real;begin\nx:=sqrt(4)end.",0xff,"x:=sqrt");
    checked_error("var x:real;begin\nx:=ln(2)end.",0xff,"x:=ln");
    checked_error("var x:real;begin\nreadln(x)end.",0xff,"readln");
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    check(code_capture(1));
    test_stage=6;test_done=255;test_complete();return 0;
}
