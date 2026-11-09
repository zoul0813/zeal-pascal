#include "compiler/internal.h"
#include "units/internal.h"
#include "program/image.h"
#include <zos_vfs.h>
#include <zos_sys.h>
volatile uint16_t test_passed,test_failed,test_index,test_stage,test_error,test_position;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_first_error,test_first_position,test_runtime_code,test_runtime_pc;
Source s;Build b;Diagnostic d;
void test_complete(void);
static void check(uint8_t ok){++test_index;if(ok)++test_passed;else {if(test_failed<64)test_failures[test_failed]=test_index;if(!test_failed){test_first_error=d.code;test_first_position=d.start;}++test_failed;}}
static void client(const char *path,uint8_t expected) {
    uint8_t ok,status=255;
    memset(&b,0,sizeof(b));memset(b.pages,255,3);b.map_page=b.sources.page=255;
    check(source_new(&s,path));check(load_source(&s,path,&d));
    ok=compile(&s,&b,&d);test_error=d.code;test_position=d.start;
    if(expected){check(!ok&&d.code==expected);}
    else {
        check(ok&&b.valid);
        if(ok){check(save_build(&s,&b,0));{uint8_t ran=run_build(&s,&b);test_runtime_code=ram_result[28];test_runtime_pc=ram_result[30]|((uint16_t)ram_result[31]<<8);check(ran&&ram_result[26]==1);}}
    }
    build_close(&b);source_close(&s);
    if(!expected&&ok){char exe[128];strcpy(exe,path);strcpy(exe+strlen(exe)-3,"COM");check(!exec(EXEC_PRESERVE_PROGRAM,exe,0,&status)&&status==0);}
}
static void saved(const char *path,uint8_t code) {
    uint8_t status=0;char exe[128];
    check(source_new(&s,path));check(load_source(&s,path,&d));check(compile(&s,&b,&d));
    if(b.valid)check(save_build(&s,&b,0));
    build_close(&b);source_close(&s);strcpy(exe,path);strcpy(exe+strlen(exe)-3,"COM");
    check(!exec(EXEC_PRESERVE_PROGRAM,exe,0,&status)&&status==code);
}
static void author(void) {
    int8_t fd;check(source_new(&s,"H:/ZOSUNIT.PAS"));check(load_source(&s,"H:/ZOSUNIT.PAS",&d));
    fd=open("H:/ZOSUNIT.ZPU",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd>=0){check(compile_unit(&s,&b,&d,fd,0));check(!close(fd));}
    build_close(&b);source_close(&s);
}
int app_main(void) {
    if(!console_open())return 1;
    test_stage=1;client("H:/SYSCALLS.PAS",0);
    test_stage=2;client("H:/NUMERIC.PAS",0);
    test_stage=3;client("H:/BADVAR.PAS",41);
    test_stage=4;client("H:/BADMAP.PAS",44);
    test_stage=5;client("H:/BADCOUNT.PAS",44);
    test_stage=6;client("H:/SHADOW.PAS",0);
    test_stage=7;client("H:/HIDDEN.PAS",41);
    test_stage=8;client("H:/QUALIFY.PAS",UNIT_INTERFACE);
    test_stage=9;client("H:/ZOSREAL.PAS",0);
    test_stage=10;saved("H:/ZEXIT.PAS",73);
    test_stage=11;saved("H:/ZOVER.PAS",42);
    test_stage=12;author();client("H:/UNITCALL.PAS",0);
    test_stage=13;client("H:/BAD/STALE.PAS",UNIT_STALE);
    test_stage=14;client("H:/ZPAGE.PAS",0);
    test_stage=15;client("H:/ZIORES.PAS",0);
    test_done=255;test_complete();return 0;
}
