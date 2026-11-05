/* Compile the actual example files and run saved native programs with replay
 * keyboard input. Each run gets a fresh capture descriptor; the database survives the two runs. */
#include "harness.h"
#include "image.h"
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s; Build b; Diagnostic d;
static char binary[128];
void test_complete(void);
void test_eof_return(void);
/* Use exactly the published native image, with the requested .bin name. */
static uint8_t publish_bin(void) {
    int8_t file;uint16_t offset=0,size,expected,i;uint8_t ok=1;
    strcpy(binary,s.path);strcpy(binary+strlen(binary)-4,".bin");
    file=open(binary,O_WRONLY|O_CREAT|O_TRUNC);if(file<0)return 0;
    while(offset<b.length&&ok) {
        size=b.length-offset;if(size>128)size=128;expected=size;
        for(i=0;i<size;++i)console_scratch[i]=image_get(&b,offset+i);
        if(write(file,console_scratch,&size)||size!=expected)ok=0;
        offset+=size;
    }
    if(close(file))ok=0;
    return ok;
}
static void replay(const char *output_path) {
    int8_t capture;
    uint8_t status=255,error,owned=pages_used();
    capture=open(output_path,O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) {
        check(swap(capture,DEV_STDOUT)==0);
        error=execute_native(binary,&status);
        if(error||status) {
            static char name[]="H:/exit0.err";int8_t fd;uint16_t length=2;
            name[7]='0'+test_stage;console_scratch[0]=error;console_scratch[1]=status;
            fd=open(name,O_WRONLY|O_CREAT|O_TRUNC);
            if(fd>=0){write(fd,console_scratch,&length);close(fd);}
        }
        check(error==0&&status==0);
        check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);
    }
    check(pages_used()==owned);
}
/* CrtExit must restore all eight cursor/scroll/rendition registers.
 * Con writes remain physical while Output is captured for the epilogue. */
static uint8_t terminal[8],original_terminal[8];
static uint8_t video_register(uint8_t reg) __naked {
    reg;
    __asm
        ld c,a
        ld a,i
        push af
        di
        in a,(0x8e)
        push af
        xor a
        out (0x8e),a
        in a,(c)
        ld b,a
        pop af
        out (0x8e),a
        pop af
        jp po,90101$
        ei
90101$:
        ld a,b
        ret
    __endasm;
}
static void video_write(uint16_t pair) __naked {
    pair;
    __asm
        ld c,l
        ld b,h
        ld a,i
        push af
        di
        in a,(0x8e)
        push af
        xor a
        out (0x8e),a
        out (c),b
        pop af
        out (0x8e),a
        pop af
        jp po,90102$
        ei
90102$:
        ret
    __endasm;
}
static void save_terminal(void) {
    uint8_t i;
    /* Deliberately different from NormVideo/ClrScr: a missing CrtExit fails. */
    video_write(0x05a1);video_write(0x09a2);video_write(0x07a3);
    video_write(0x03a4);video_write(0x07a5);video_write(0x70a8);
    for(i=0;i<8;++i)terminal[i]=video_register(0xa1+i);
    check(terminal[0]==5&&terminal[1]==9&&terminal[2]==7&&
          terminal[3]==3&&terminal[4]==7&&terminal[7]==0x70);
}
static void check_terminal(void) {
    uint8_t i,ok=1;
    for(i=0;i<8;++i)if(terminal[i]!=video_register(0xa1+i))ok=0;
    check(ok);
}
static uint8_t prepare(const char *path) {
    if(!load_source(&s,path,&d)){check(0);return 0;}
    check(1);
    if(!compile(&s,&b,&d)) {
        check(0);output("EXAMPLE COMPILE ");output(path);output(" error ");
        output_number(d.code);output(" at ");output_number(d.start);output("\n");return 0;
    }
    check(b.valid);check(save_build(&s,&b,0));
    if(publish_bin()){check(1);return 1;}else {check(0);return 0;}
}
static uint8_t example(const char *path,const char *output_path) {
    uint8_t ready=prepare(path);
    /* Disk execution needs no cached compiler image. Release it as the IDE does. */
    build_close(&b);
    if(ready)replay(output_path);
    return ready;
}
int app_main(void) {
    uint8_t ready,i;
    if(!console_open())return 1;
    check(source_new(&s,"H:/lighthouse/lighthouse.pas"));
    test_stage=1;
    check(chdir("H:/lighthouse") == 0);
    for(i=0;i<8;++i)original_terminal[i]=video_register(0xa1+i);
    save_terminal();
    ready=example("H:/lighthouse/lighthouse.pas","H:/adventure.out");build_close(&b);
    if(ready) {
        check_terminal();save_terminal();replay("H:/adventure-quit.out");check_terminal();
        save_terminal();replay("H:/adventure-eof.out");check_terminal();test_eof_return();
        save_terminal();replay("H:/adventure-win-eof.out");check_terminal();test_eof_return();
    }
    for(i=0;i<8;++i)video_write(((uint16_t)original_terminal[i]<<8)|(0xa1+i));
    test_stage=2;
    check(chdir("H:/partsbin") == 0);
    ready=prepare("H:/partsbin/partsbin.pas");build_close(&b);
    if(ready) {
        replay("H:/create-eof.out");test_eof_return();replay("H:/parts.out");
    }
    if(ready)replay("H:/reopen.out");build_close(&b);
    test_stage=3;
    check(chdir("H:/loan") == 0);
    ready=example("H:/loan/loan.pas","H:/loan.out");
    if(ready)replay("H:/zero.out");build_close(&b);
    test_stage=4;
    check(chdir("H:/concord") == 0);
    example("H:/concord/concord.pas","H:/concord.out");build_close(&b);
    test_stage=5;
    check(chdir("H:/life") == 0);
    example("H:/life/life.pas","H:/life.out");build_close(&b);
    test_stage=6;
    check(chdir("H:/tasks") == 0);
    example("H:/tasks/tasks.pas","H:/tasks.out");build_close(&b);
    test_stage=7;check(chdir("H:/calc")==0);
    ready=example("H:/calc/calc.pas","H:/calc.out");
    if(ready)replay("H:/calc-errors.out");build_close(&b);
    test_stage=8;check(chdir("H:/queens")==0);
    ready=example("H:/queens/queens.pas","H:/queens.out");
    if(ready)replay("H:/queen-counts.out");build_close(&b);
    test_stage=9;check(chdir("H:/measure")==0);
    prepare("H:/measure/stats.pas");build_close(&b);
    ready=example("H:/measure/measure.pas","H:/measure.out");
    if(ready) {
        check(rm("H:/measure/stats.bin")==0);replay("H:/missing.out");replay("H:/eof.out");test_eof_return();
    }
    build_close(&b);
    example("H:/measure/stats.pas","H:/measure-reopen.out");build_close(&b);
    source_close(&s);check(pages_used()==code_required);
    test_done=255;test_complete();return 0;
}
