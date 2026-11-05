#include "harness.h"
#include "compiler.h"

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
    /* Current transport rejects a low-page buffer before reading its bytes,
     * even with I-: namespace2/FF is a memory-authority failure, not I/O. */
    checked_error("var f:file;low:array[0..127]of byte absolute $6000;\n"
        "actual:integer;begin assign(f,'H:/LOW.DAT');rewrite(f);\n"
        "{$I-}blockwrite(f,low,1,actual);{$I+}close(f)end.",0xff,"blockwrite");
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
