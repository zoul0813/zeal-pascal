#include "harness.h"
#include "units/internal.h"
#include "program/decode.h"
#include <zos_time.h>
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done,screen_final_pages,screen_final_cached;
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
    int8_t capture;uint8_t cell;
    if(!console_open())return 1;
    check(source_new(&s,"H:/src/CRT.PAS"));
    /* A Screen-free program requires no library; names require USES. */
    check(source_import(&s,(const uint8_t*)"begin clrscr end.",17,&d));
    check(!compile(&s,&b,&d)&&d.code==41);build_close(&b);
    /* A corrupt local unit must not be bypassed by the installed copy. */
    check(load("begin ClrScr end."));check(!compile(&s,&b,&d)&&d.code==248);build_close(&b);
    check(!rm("H:/src/CRT.ZPU"));
    /* Unused imports add no executable bytes or execution requirement. */
    {
        uint16_t plain=0,one=0;uint8_t plain_flags=0;
        check(source_import(&s,(const uint8_t*)"begin end.",10,&d));check(compile(&s,&b,&d));
        if(b.valid){plain=b.length;plain_flags=far_get(b.pages[0],18);}build_close(&b);
        check(load("begin end."));check(compile(&s,&b,&d));
        if(b.valid)check(b.length==plain&&far_get(b.pages[0],18)==plain_flags);build_close(&b);
        check(load("begin Crt.ClrEol end."));check(compile(&s,&b,&d));
        if(b.valid) {one=b.length;check(one>plain);}build_close(&b);
        check(load("begin ClrEol;ClrScr;CrtInit;CrtExit end."));check(compile(&s,&b,&d));
        if(b.valid)check(b.length>one);build_close(&b);
        rejected("var N:Integer;begin N:=Addr(ClrScr)end.",249);
    }
    /* Ordinary authored units use the same library-directory fallback. */
    {
        static const char unit[]="unit Tiny;interface function Plus(N:Integer):Integer;"
            "implementation function Plus(N:Integer):Integer;begin Plus:=N+1 end;end.";
        static const char client[]="uses Tiny;var Z:Integer;begin Z:=Plus(41);if Z<>42 then Z:=1 div(Z-Z)end.";
        int8_t fd;
        strcpy(s.path,"H:/TINY.PAS");check(source_import(&s,(const uint8_t*)unit,sizeof(unit)-1,&d));
        fd=open("H:/TINY.ZPU",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
        if(fd>=0){check(compile_unit(&s,&b,&d,fd,0));check(!close(fd));}build_close(&b);
        strcpy(s.path,"H:/src/CRT.PAS");check(source_import(&s,(const uint8_t*)client,sizeof(client)-1,&d));
        check(compile(&s,&b,&d));if(b.valid)check(run_build(&s,&b)&&ram_result[26]==1);build_close(&b);
        check(!rm("H:/TINY.ZPU"));
    }
    capture=open("H:/CRT.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0)check(swap(capture,DEV_STDOUT)==0);
    test_stage=53;
    execution("procedure P;var A,B,C:Integer;begin A:=123;B:=456;{$I-}reset(input);\n"
              "if WhereX<>0 then halt;if WhereY<>0 then halt;C:=IOResult;{$I+}\n"
              "if(C<>$22)or(A<>123)or(B<>456)then halt;writeln('SCREEN-QUERY-OK')end;begin P end.",1,0);
    execution_file("H:/src/CRT.PAS",1,0,0);
    execution_file("H:/src/COLORS.PAS",1,0,0);
    execution("var code:integer;begin lowvideo;{$I-}reset(input);normvideo;\n"
              "delay(25);code:=ioresult;{$I+}if(code=$22)and(port[$A5]=7)"
              "then writeln('CRT-IO-OK')else writeln('CRT-IO-FAIL')end.",1,0);
    execution("var code:integer;begin port[$9C]:=1;delay(25);\n{$I-}"
              "clrscr;code:=ioresult;port[$9C]:=0;delay(25);{$I+}"
              "if code<>$20 then halt end.",1,0);
    checked_error("begin port[$9C]:=1;delay(25);\nclrscr end.",0x20,"clrscr");
    checked_error("begin port[$9C]:=1;delay(25);\nif WhereX=0 then halt end.",0x20,"if");
    __asm
        xor a
        out (0x9c),a
    __endasm;
    check(msleep(25)==0);
    checked_error("begin\ndelay(-1)end.",0x91,"delay");
    checked_error("begin\ngotoxy(0,1)end.",0x91,"gotoxy");
    checked_error("begin\ngotoxy(1,41)end.",0x91,"gotoxy");
    checked_error("begin {$R-}\ngotoxy(257,1)end.",0x91,"gotoxy");
    checked_error("begin\ntextcolor(16)end.",0x91,"textcolor");
    checked_error("begin\ntextbackground(-1)end.",0x91,"textbackground");
    checked_error("begin {$R-}\ntextbackground(256)end.",0x91,"textbackground");
    rejected("begin textcolor(1.0)end.",44);
    rejected("begin textbackground('R')end.",44);
    rejected("begin textcolor end.",4);
    rejected("begin textbackground(1,2)end.",5);
    rejected("begin writeln(Blink)end.",41);
    execution("const White=3;function WhereX:integer;begin WhereX:=White end;\n"
              "procedure TextColor(n:integer);begin end;\n"
              "begin TextColor(WhereX);if WhereX<>3 then halt end.",1,0);
    rejected("begin delay('X')end.",44);
    rejected("begin gotoxy(1,true)end.",44);
    rejected("begin gotoxy(1)end.",3);
    rejected("begin delay end.",4);
    rejected("begin clrscr(1)end.",5);
    checked_error("var z:integer;begin ClrScr;GotoXY(1,1);\n"
                  "TextBackground(Black);TextColor(Black);z:=0;writeln(7 div z)end.",2,"writeln");
    check(physical_cell(0x4000)=='R'&&physical_cell(0x5000)==15);
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
    /* A production Chain child can retain imported Screen fragments. */
    {
        CompileOptions options={0x9000,0xfbffu,COMPILE_CHAIN};uint8_t status=255;
        strcpy(s.path,"H:/src/SCHAIN.PAS");
        check(load("var N:Integer;begin N:=0;CrtExit;GotoXY(7,9);CrtInit;GotoXY(2,3);CrtExit;\n"
                   "if(WhereX<>7)or(WhereY<>9)then N:=1 div N;writeln('SCREEN-CHAIN-OK')end."));
        check(compile_project(&s,&b,&d,0,&options));if(b.valid)check(save_build(&s,&b,0));build_close(&b);
        strcpy(s.path,"H:/src/SPARENT.PAS");
        check(load("var F:File;begin Assign(F,'H:/src/SCHAIN.CHN');Chain(F)end."));
        options.kind=COMPILE_PROGRAM;check(compile_project(&s,&b,&d,0,&options));
        if(b.valid){check(save_build(&s,&b,0));check(run_saved(&s,&b,&status)&&status==0);}build_close(&b);
        /* A fresh standalone observer also checks this pair after exit. */
        strcpy(s.path,"H:/src/CRT.PAS");
    }
    if(capture>=0){check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    /* Ordinary Write with inherited physical stdout must see the same CRT
     * cursor/color state as Con, in both RAM and self-contained COM. */
    check(load("begin TextBackground(Blue);TextColor(Yellow);clrscr;gotoxy(10,5);crtinit;\n"
               "TextBackground(White);TextColor(Red);gotoxy(20,6);write('Y');crtexit;write('X');\nTextBackground(Green);GotoXY(80,40);write('Z');write('N')end."));
    check(compile(&s,&b,&d));
    if(b.valid) {
        uint8_t status=255;
        check(save_build(&s,&b,0));
        check(run_build(&s,&b)&&ram_result[26]==1);
        check(physical_cell(0x4149)=='X'&&physical_cell(0x5149)==0x1e);
        check(physical_cell(0x41a3)=='Y'&&physical_cell(0x51a3)==0xf4);
        check(physical_cell(0x4000)=='N'&&physical_cell(0x5000)==0x2e);
        for(cell=1;cell<80;++cell)check(physical_cell(0x4000+cell)==32&&physical_cell(0x5000+cell)==0x2e);
        check(run_saved(&s,&b,&status)&&status==0);
        check(physical_cell(0x4149)=='X'&&physical_cell(0x5149)==0x1e);
        check(physical_cell(0x41a3)=='Y'&&physical_cell(0x51a3)==0xf4);
        check(physical_cell(0x4000)=='N'&&physical_cell(0x5000)==0x2e);
        for(cell=1;cell<80;++cell)check(physical_cell(0x4000+cell)==32&&physical_cell(0x5000+cell)==0x2e);
        build_close(&b);
    }
    /* Saved code is self-contained after deleting the shipped artifact. */
    check(load("var N:Integer;begin GotoXY(7,9);if WhereX<>7 then N:=1 div N end."));
    check(compile(&s,&b,&d));
    if(b.valid) {
        uint8_t status=255;
        check(save_build(&s,&b,0));check(!rm("H:/CRT.ZPU"));
        check(run_saved(&s,&b,&status)&&status==0);
        build_close(&b);
    }
    check(load("begin ClrScr end."));check(!compile(&s,&b,&d)&&d.code==250);build_close(&b);
    check(source_import(&s,(const uint8_t*)"begin end.",10,&d));
    check(compile(&s,&b,&d));build_close(&b);
    source_close(&s);screen_final_pages=pages_used();screen_final_cached=cached_code_pages();
    check(screen_final_pages==screen_final_cached);
    test_stage=6;test_done=255;test_complete();return 0;
}
