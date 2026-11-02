/* Inline file blocks across a code-only handoff, explicit Close and native
 * terminal/Execute descriptor cleanup. This does not replace caller code. */
#include "file_runtime.h"
#include <string.h>
#include <zos_vfs.h>
#ifndef __SDCC
#include <stdio.h>
uint8_t zf_user_memory[65536];
#define VM(at) (zf_user_memory+(at))
extern uint8_t model_pages,model_handles,model_close_error,model_write_error;
extern uint8_t model_free_error;
extern uint16_t model_open_calls,model_close_calls,model_read_calls,model_write_calls;
#else
#define VM(at) ((uint8_t*)(at))
extern void test_complete(void);
extern void file_probe(uint8_t cleanup,void *job) ZFILE_CALL;
uint8_t file_probe_before[7],file_probe_after[7],file_probe_ok;
#endif
volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
static FileJob job;
static FileCleanup ending;
static uint8_t saved[ZF_FILE_BYTES];
static void check_at(uint8_t ok,uint16_t line) {
    if(ok)++test_passed;
    else {if(test_index<64)test_failures[test_index++]=line;++test_failed;
#ifndef __SDCC
        printf("Chain file check at line %u\n",line);
#endif
    }
}
#define check(ok) check_at((ok),__LINE__)
static void put(uint16_t at,uint16_t value) {VM(at)[0]=(uint8_t)value;VM(at)[1]=(uint8_t)(value>>8);}
static uint16_t call(uint16_t operation) {
    job.operation=operation;
#ifdef __SDCC
    file_probe(0,&job);check(file_probe_ok&&!memcmp(file_probe_before,file_probe_after,7));
#else
    zf_dispatch(&job);
#endif
    return job.error;
}
static uint16_t cleanup(uint16_t operation) {
    ending.operation=operation;
#ifdef __SDCC
    file_probe(1,&ending);check(file_probe_ok&&!memcmp(file_probe_before,file_probe_after,7));
#else
    zf_cleanup(&ending);check(!model_pages);
#endif
    return ending.error;
}
static void select(uint16_t object) {job.object=object;job.descriptor=0xd100;job.data=0xd200;job.count=0;}
static void named(uint16_t object,const char *path) {
    select(object);job.data=0xd400;
    VM(0xd400)[0]=(uint8_t)strlen(path);memcpy(VM(0xd401),path,strlen(path));
    check(call(ZF_ASSIGN)==0);job.data=0xd200;
}
static void output(uint16_t object,uint16_t value) {
    select(object);put(0xd200,value);check(call(ZF_WRITE)==0&&job.actual==1);
}
static void parent(void) {
    memset(&job,0,sizeof(job));
    job.heap_floor=0xe400;job.heap_frontier=0xe4b2;
    job.static_end=0xe000;job.stack_pointer=0xe700;job.code_origin=0xd100;
    job.standard_table=0xd180;
    put(0xe400,176);VM(0xd100)[0]=1;put(0xd101,2);
    VM(0xd103)[0]=0;put(0xd104,0);
    put(0xd180,2);put(0xd182,0xc400);put(0xd184,2);put(0xd186,0xc500);put(0xd188,1);
    named(0xc000,"H:/CGLOBAL.DAT");check(call(ZF_REWRITE)==0);output(0xc000,11);
    named(0xe402,"H:/CHEAP.DAT");check(call(ZF_REWRITE)==0);output(0xe402,33);
    named(0xc300,"H:/CSTATIC.DAT");check(call(ZF_REWRITE)==0);output(0xc300,55);check(call(ZF_CLOSE)==0);
    named(0xf650,"H:/CLOCAL.DAT");check(call(ZF_REWRITE)==0);output(0xf650,66);check(call(ZF_CLOSE)==0);
    named(0xc600,"H:/CPRIV.DAT");check(call(ZF_REWRITE)==0);output(0xc600,77);check(call(ZF_CLOSE)==0);
}
static void path_cases(void) {
#ifndef __SDCC
    uint16_t opens=model_open_calls,reads=model_read_calls,closes=model_close_calls,writes=model_write_calls;
#endif
    uint16_t i;
    select(0xc000);memcpy(saved,VM(0xc000),ZF_FILE_BYTES);
    job.data=0xd500;job.count=128;memset(VM(0xd500),0xa5,128);
    check(call(ZF_TRANSFER_PATH)==0&&!strcmp((char*)VM(0xd500),"H:/CGLOBAL.DAT")&&job.actual==14);
    check(!memcmp(saved,VM(0xc000),ZF_FILE_BYTES));
    for(i=15;i<128;++i)if(VM(0xd500+i)[0]!=0xa5)break;check(i==128);
    job.count=14;memset(VM(0xd500),0xa5,128);check(call(ZF_TRANSFER_PATH)==0x20&&!job.actual);
    for(i=0;i<128;++i)if(VM(0xd500+i)[0]!=0xa5)break;check(i==128);
    job.count=128;job.data=0xdfff;VM(0xdfff)[0]=0xa5;check(call(ZF_TRANSFER_PATH)==0x20&&VM(0xdfff)[0]==0xa5);
    job.data=0x7000;check(call(ZF_TRANSFER_PATH)==0x20);
    select(0xc400);job.descriptor=0xd103;job.data=0xd500;job.count=128;
    check(call(ZF_TRANSFER_PATH)==0x20&&!job.stream);
#ifndef __SDCC
    check(opens==model_open_calls&&reads==model_read_calls&&closes==model_close_calls&&writes==model_write_calls);
#endif
    select(0xc000);check(call(ZF_POSITION)==0&&job.result==1);
    select(0xc800);job.data=0xd400;VM(0xd400)[0]=127;memcpy(VM(0xd401),"t:/",3);
    for(i=3;i<127;++i)VM(0xd401+i)[0]=i%16==2?'/':'a'+i%26;
    check(call(ZF_ASSIGN)==0);job.data=0xd500;job.count=128;memset(VM(0xd500),0xa5,129);
    check(call(ZF_TRANSFER_PATH)==0&&job.actual==127&&VM(0xd500)[0]=='T'&&VM(0xd57f)[0]==0&&VM(0xd580)[0]==0xa5);
    check(!memcmp(VM(0xd501),VM(0xd402),126));
    select(0xc900);memset(VM(0xc900),0xa5,ZF_FILE_BYTES);job.data=0xd500;job.count=128;
    check(call(ZF_TRANSFER_PATH)!=0);
}
static void execute_files(void) {
    struct {FileCleanup job;uint8_t guard[8];} executing;
    uint16_t i;
    memcpy(saved,VM(0xc000),ZF_FILE_BYTES);memset(&executing,0xa5,sizeof(executing));
    memset(&executing.job,0,sizeof(FileCleanup));executing.job.operation=ZF_END_EXECUTE;
#ifdef __SDCC
    file_probe(1,&executing.job);check(file_probe_ok&&!memcmp(file_probe_before,file_probe_after,7));
#else
    zf_cleanup(&executing.job);check(!model_pages);
#endif
    check(!executing.job.error&&!memcmp(saved,VM(0xc000),ZF_FILE_BYTES));
    for(i=0;i<8;++i)if(executing.guard[i]!=0xa5)break;check(i==8);
    select(0xc000);check(call(ZF_RESET)==0);check(call(ZF_SIZE)==0&&job.result==0);check(call(ZF_CLOSE)==0);
    select(0xe402);check(call(ZF_RESET)==0);check(call(ZF_SIZE)==0&&job.result==0);check(call(ZF_CLOSE)==0);
    check(cleanup(ZF_END_PROGRAM)==0);
}
static void child(void) {
    memcpy(saved,VM(0xc000),ZF_FILE_BYTES);
    /* Code-only handoff preserves inline associations. There is no local or
     * object lifetime cleanup; callers explicitly Close files before retiring
     * their storage. Rebinding standard blocks leaves ordinary files alone. */
    put(0xd182,0xc700);put(0xd184,1);put(0xd186,0xc500);put(0xd188,2);
    select(0xc000);check(call(ZF_POSITION)==0&&job.result==1);
    check(!memcmp(saved,VM(0xc000),ZF_FILE_BYTES));
    output(0xc000,22);check(call(ZF_CLOSE)==0);output(0xe402,44);check(call(ZF_CLOSE)==0);
    select(0xc700);job.descriptor=0xd103;check(call(ZF_TEXT_BEGIN)==0&&job.stream==1);
    select(0xc500);job.descriptor=0xd103;VM(0xd200)[0]=1;VM(0xd201)[0]='X';
    check(call(ZF_TEXT_WRITE)==0&&job.stream==2);
    check(cleanup(ZF_END_PROGRAM)==0);
}
static void file_bytes(const char *name,const uint8_t *expected,uint16_t bytes) {
    int8_t fd=open(name,O_RDONLY);uint16_t n=bytes+1;
    check(fd>=0);if(fd<0)return;
    check(read(fd,VM(0xd500),&n)==0&&n==bytes&&!memcmp(VM(0xd500),expected,bytes));check(close(fd)==0);
}
static void results(void) {
    static const uint8_t global[]={2,0,2,0,11,0,22,0},heap[]={2,0,2,0,33,0,44,0};
    static const uint8_t code[]={1,0,2,0,55,0},local[]={1,0,2,0,66,0},trailing[]={1,0,2,0,77,0};
    file_bytes("H:/CGLOBAL.DAT",global,sizeof(global));file_bytes("H:/CHEAP.DAT",heap,sizeof(heap));
    file_bytes("H:/CSTATIC.DAT",code,sizeof(code));file_bytes("H:/CLOCAL.DAT",local,sizeof(local));file_bytes("H:/CPRIV.DAT",trailing,sizeof(trailing));
}
#ifndef __SDCC
static void faults(void) {
    parent();model_close_error=ERR_FAILURE;
    check(cleanup(ZF_END_EXECUTE)==0xf0&&ending.detail==ERR_FAILURE&&model_handles==0&&model_pages==0);
    model_close_error=0;check(cleanup(ZF_END_PROGRAM)==0&&model_handles==0);
    select(0xc000);check(call(ZF_RESET)==0);check(call(ZF_SIZE)==0&&job.result==0);check(call(ZF_CLOSE)==0);
}
#endif
int main(void) {
    parent();execute_files();
#ifndef __SDCC
    faults();check(model_pages==0&&model_handles==0);
#endif
    parent();path_cases();child();results();
#ifndef __SDCC
    printf("%u chain file assertions passed, %u failed\n",test_passed,test_failed);
#else
    test_done=255;test_complete();
#endif
    return test_failed?1:0;
}
