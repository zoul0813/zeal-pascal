#include "harness.h"
#include <zos_time.h>
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s;Build b;Diagnostic d;
void test_complete(void);
/* Independent framebuffer oracle while the C supervisor runs above8000. */
static uint8_t physical_cell(uint16_t address) __naked {
    __asm
        ld a,#0x40
        in a,(0xf1)
        push af
        ld a,#0x40
        out (0xf1),a
        ld b,(hl)
        pop af
        out (0xf1),a
        ld a,b
        ret
    __endasm;
}
static void rejected(const char *text,uint8_t code) {
    check(load(text));check(!compile(&s,&b,&d)&&d.code==code);build_close(&b);
}
int app_main(void) {
    int8_t capture;
    if(!console_open())return 1;
    check(source_new(&s,"H:/CRT.PAS"));
    capture=open("H:/CRT.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=53;
    execution_file("H:/CRT.PAS",1,0,0);
    execution("var code:integer;begin lowvideo;{$I-}reset(input);normvideo;\n"
              "delay(25);code:=ioresult;{$I+}if(code=$22)and(port[$A5]=7)"
              "then writeln('CRT-IO-OK')else writeln('CRT-IO-FAIL')end.",1,0);
    execution("var code:integer;begin port[$9C]:=1;delay(25);\n{$I-}"
              "clrscr;code:=ioresult;port[$9C]:=0;delay(25);{$I+}"
              "if code<>$20 then halt end.",1,0);
    checked_error("begin port[$9C]:=1;delay(25);\nclrscr end.",0x20,"clrscr");
    __asm
        xor a
        out (0x9c),a
    __endasm;
    check(msleep(25)==0);
    checked_error("begin\ndelay(-1)end.",0x91,"delay");
    checked_error("begin\ngotoxy(0,1)end.",0x91,"gotoxy");
    checked_error("begin\ngotoxy(1,41)end.",0x91,"gotoxy");
    checked_error("begin {$R-}\ngotoxy(257,1)end.",0x91,"gotoxy");
    rejected("begin delay('X')end.",23);
    rejected("begin gotoxy(1,true)end.",23);
    rejected("begin gotoxy(1)end.",3);
    rejected("begin delay end.",4);
    rejected("begin clrscr(1)end.",1);
    /* A terminal session belongs to a Run, unlike writable typed constants. */
    check(load("const first:boolean=true;begin if first then begin first:=false;\n"
               "gotoxy(2,3);crtinit;halt end;gotoxy(5,6);crtexit;\n"
               "if(port[$A2]=4)and(port[$A1]=5)then writeln('CRT-RESTART-OK')"
               "else writeln('CRT-RESTART-FAIL')end."));
    check(compile(&s,&b,&d));
    if(b.valid) {
        check(run_build(&s,&b)&&ram_result[26]==4);
        check(run_build(&s,&b)&&ram_result[26]==1);
        check(build_check(&s,&b));build_close(&b);
    }
    /* User declarations retain ordinary shadowing of predefined routines. */
    execution("procedure delay;begin end;begin delay end.",1,0);
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    /* Ordinary Write with inherited physical stdout must see the same CRT
     * cursor/color state as Con, in both RAM and self-contained COM. */
    check(load("begin clrscr;gotoxy(10,5);lowvideo;crtinit;\n"
               "gotoxy(20,6);write('Y');crtexit;write('X')end."));
    check(compile(&s,&b,&d));
    if(b.valid) {
        uint8_t status=255;
        check(save_build(&s,&b,0));
        check(run_build(&s,&b)&&ram_result[26]==1);
        check(physical_cell(0x4149)=='X'&&physical_cell(0x5149)==7);
        check(physical_cell(0x41a3)=='Y'&&physical_cell(0x51a3)==15);
        check(run_saved(&s,&b,&status)&&status==0);
        check(physical_cell(0x4149)=='X'&&physical_cell(0x5149)==7);
        check(physical_cell(0x41a3)=='Y'&&physical_cell(0x51a3)==15);
        build_close(&b);
    }
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
