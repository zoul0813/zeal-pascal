/* File-owner side of a code-only handoff. This does not replace caller code:
 * it proves private path authority and which file lifetimes survive/retire. */
#include "file_runtime.h"
#include <string.h>
#include <zos_vfs.h>
#ifndef __SDCC
#include <stdio.h>
uint8_t zf_user_memory[65536];
#define VM(at) (zf_user_memory+(at))
extern uint8_t model_pages,model_handles,model_close_error,model_write_error;
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
static uint8_t token[16],private_token[16];
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
static uint16_t get(uint16_t at) {return VM(at)[0]|((uint16_t)VM(at)[1]<<8);}
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
    ending.operation=operation;ending.owner=0xc100;ending.end=0xe422;
#ifdef __SDCC
    file_probe(1,&ending);check(file_probe_ok&&!memcmp(file_probe_before,file_probe_after,7));
#else
    zf_cleanup(&ending);
#endif
    return ending.error;
}
static void select(uint16_t object) {job.object=object;job.descriptor=0xc100;job.data=0xc200;job.count=0;}
static void named(uint16_t object,const char *path) {
    select(object);job.data=0xc400;
    VM(0xc400)[0]=(uint8_t)strlen(path);memcpy(VM(0xc401),path,strlen(path));
    check(call(ZF_ASSIGN)==0);job.data=0xc200;
}
static void output(uint16_t object,uint16_t value) {
    select(object);put(0xc200,value);check(call(ZF_WRITE)==0&&job.actual==1);
}
static void parent(void) {
    memset(&job,0,sizeof(job));
    job.frame=0xf700;job.heap_floor=0xe400;job.heap_frontier=0xe422;
    job.static_end=0xe000;job.stack_pointer=0xe700;job.code_origin=0xc100;
    job.standard_table=0xc180;
    memset(VM(0xc000),0xa5,0x80);memset(VM(0xc300),0xa5,16);memset(VM(0xe402),0xa5,32);memset(VM(0xf650),0xa5,16);
    put(0xe400,32);put(0xf700,0);put(0xf6fc,0xf700);put(0xf6f8,0xf640);
    VM(0xc100)[0]=1;put(0xc101,2);VM(0xc103)[0]=0;put(0xc104,0);
    put(0xc180,2);put(0xc182,0xc020);put(0xc184,2);put(0xc186,0xc040);put(0xc188,1);
    named(0xc000,"H:/CGLOBAL.DAT");check(call(ZF_REWRITE)==0);output(0xc000,11);
    named(0xe402,"H:/CHEAP.DAT");check(call(ZF_REWRITE)==0);output(0xe402,33);
    named(0xc300,"H:/CSTATIC.DAT");check(call(ZF_REWRITE)==0);output(0xc300,55);
    named(0xf650,"H:/CLOCAL.DAT");check(call(ZF_REWRITE)==0);output(0xf650,66);
    named(0xc060,"H:/CPRIV.DAT");check(call(ZF_REWRITE)==0);output(0xc060,77);
    memcpy(private_token,VM(0xc060),16);
}
static void path_cases(void) {
#ifndef __SDCC
    uint16_t opens=model_open_calls,reads=model_read_calls,closes=model_close_calls,writes=model_write_calls;
#endif
    uint16_t i;
    select(0xc000);memcpy(token,VM(0xc000),16);job.data=0xc500;job.count=128;memset(VM(0xc500),0xa5,128);
    check(call(ZF_TRANSFER_PATH)==0&&!strcmp((char*)VM(0xc500),"H:/CGLOBAL.DAT")&&job.actual==14);
    check(!memcmp(token,VM(0xc000),16));
    for(i=15;i<128;++i)if(VM(0xc500+i)[0]!=0xa5)break;
    check(i==128);
    job.count=14;memset(VM(0xc500),0xa5,128);check(call(ZF_TRANSFER_PATH)==0x20&&!job.actual);
    for(i=0;i<128;++i)if(VM(0xc500+i)[0]!=0xa5)break;check(i==128);
    job.count=128;job.data=0xdfff;VM(0xdfff)[0]=0xa5;check(call(ZF_TRANSFER_PATH)==0x20&&VM(0xdfff)[0]==0xa5);
    job.data=0x7000;check(call(ZF_TRANSFER_PATH)==0x20);
    memcpy(VM(0xc080),token,16);job.object=0xc080;job.data=0xc500;
    check(call(ZF_TRANSFER_PATH)!=0&&VM(0xc500)[0]==0xa5); /* copied token has no authority */
    select(0xc020);job.descriptor=0xc103;job.data=0xc500;job.count=128;
    check(call(ZF_TRANSFER_PATH)==0x20&&!job.stream);
#ifndef __SDCC
    check(opens==model_open_calls&&reads==model_read_calls&&closes==model_close_calls&&writes==model_write_calls);
#endif
    select(0xc000);check(call(ZF_POSITION)==0&&job.result==1); /* still open, same position */
    /* Maximum canonical path, preserved case, assigned-but-closed and no
     * filesystem lookup. Its missing directories must not be created. */
    select(0xc080);job.data=0xc400;VM(0xc400)[0]=127;
    memcpy(VM(0xc401),"t:/",3);
    for(i=3;i<127;++i)VM(0xc401+i)[0]=i%16==2?'/':'a'+i%26;
    check(call(ZF_ASSIGN)==0);job.data=0xd000;job.count=128;memset(VM(0xd000),0xa5,129);
    check(call(ZF_TRANSFER_PATH)==0&&job.actual==127&&VM(0xd000)[0]=='T'&&VM(0xd07f)[0]==0&&VM(0xd080)[0]==0xa5);
    check(!memcmp(VM(0xd001),VM(0xc402),126));
    select(0xc0a0);memset(VM(0xc0a0),0xa5,16);job.data=0xd000;job.count=128;
    check(call(ZF_TRANSFER_PATH)!=0); /* unassigned variables cannot supply a path */
    job.descriptor=0xc103;job.data=0xc400;VM(0xc400)[0]=4;memcpy(VM(0xc401),"cOn:",4);
    check(call(ZF_ASSIGN)==0);job.data=0xd000;job.count=128;
    check(call(ZF_TRANSFER_PATH)==0x20&&!job.stream); /* logical devices are not code files */
}
static void child(void) {
    uint16_t old_generation=get(0xc064);
    memcpy(token,VM(0xc000),16);
    check(cleanup(ZF_END_CHAIN)==0);
    check(!memcmp(token,VM(0xc000),16));
    job.standard_table=0; /* Inspect retired records without running old program initialization. */
    /* No old frame/typed-constant owner survives, even with its old token. */
    select(0xc300);check(call(ZF_POSITION)!=0);
    select(0xf650);check(call(ZF_POSITION)!=0);
    /* New compiler table puts Input over a trailing private typed file and
     * Output at the parent's old Input address. First ordinary operation must
     * finalize the displaced private file and recreate canonical standards. */
    put(0xc180,2);put(0xc182,0xc060);put(0xc184,1);put(0xc186,0xc040);put(0xc188,2);
    job.frame=0;job.standard_table=0xc180;
    /* A malformed new table must reject before closing a surviving file. */
    put(0xc182,0xc000);put(0xc186,0xc000);
    select(0xc000);check(call(ZF_POSITION)==0x1ff);
    put(0xc182,0xc060);put(0xc186,0xc040);
    select(0xc000);check(call(ZF_POSITION)==0&&job.result==1);
    check(VM(0xc069)[0]==1&&get(0xc064)!=old_generation);
    output(0xc000,22);output(0xe402,44);
    select(0xc060);job.descriptor=0xc103;check(call(ZF_TEXT_BEGIN)==0&&job.stream==1);
    select(0xc040);job.descriptor=0xc103;job.data=0xc200;VM(0xc200)[0]=1;VM(0xc201)[0]='X';
    check(call(ZF_TEXT_WRITE)==0&&job.stream==2);
    /* A copied parent file token cannot close a replacement standard stream. */
    memcpy(VM(0xc080),private_token,16);select(0xc080);check(call(ZF_CLOSE)!=0);
    check(cleanup(ZF_END_PROGRAM)==0);
}
static void file_bytes(const char *name,const uint8_t *expected,uint16_t bytes) {
    int8_t fd=open(name,O_RDONLY);uint16_t n=bytes+1;
    check(fd>=0);if(fd<0)return;
    check(read(fd,VM(0xc500),&n)==0&&n==bytes&&!memcmp(VM(0xc500),expected,bytes));check(close(fd)==0);
}
static void results(void) {
    static const uint8_t global[]={2,0,2,0,11,0,22,0},heap[]={2,0,2,0,33,0,44,0};
    static const uint8_t code[]={1,0,2,0,55,0},local[]={1,0,2,0,66,0},trailing[]={1,0,2,0,77,0};
    file_bytes("H:/CGLOBAL.DAT",global,sizeof(global));file_bytes("H:/CHEAP.DAT",heap,sizeof(heap));
    file_bytes("H:/CSTATIC.DAT",code,sizeof(code));file_bytes("H:/CLOCAL.DAT",local,sizeof(local));file_bytes("H:/CPRIV.DAT",trailing,sizeof(trailing));
}
#ifndef __SDCC
static void faults(void) {
    uint16_t i;
    for(i=0;i<2;++i) {
        parent();model_close_error=i?ERR_NO_SUCH_ENTRY:ERR_FAILURE;
        check(cleanup(ZF_END_CHAIN)==(i?0xff:0xf0));check(ending.detail==model_close_error);
        check(model_handles==3);model_close_error=0;
        /* Failed handoff keeps surviving global/heap files usable; all dying
         * records were attempted and their descriptors revoked exactly once. */
        select(0xc000);check(call(ZF_POSITION)==0&&job.result==1);output(0xc000,22);output(0xe402,44);
        select(0xc300);check(call(ZF_POSITION)!=0);select(0xf650);check(call(ZF_POSITION)!=0);
        check(cleanup(ZF_END_PROGRAM)==0&&model_handles==0&&model_pages==0);
        results();
    }
    parent();model_write_error=ERR_FAILURE;
    check(cleanup(ZF_END_CHAIN)==0xf0&&ending.detail==ERR_FAILURE&&model_handles==3);
    model_write_error=0;check(cleanup(ZF_END_PROGRAM)==0&&model_handles==0&&model_pages==0);
    parent();check(cleanup(ZF_END_CHAIN)==0);
    put(0xc180,2);put(0xc182,0xc060);put(0xc184,1);put(0xc186,0xc040);put(0xc188,2);
    job.frame=0;model_close_error=ERR_FAILURE;
    select(0xc000);check(call(ZF_POSITION)==0xf0&&job.detail==ERR_FAILURE&&model_handles==2);
    model_close_error=0;
    check(call(ZF_POSITION)==0&&job.result==1); /* retry creates the missing standard safely */
    output(0xc000,22);output(0xe402,44);
    check(cleanup(ZF_END_PROGRAM)==0&&model_handles==0&&model_pages==0);results();
}
#endif
int main(void) {
    parent();path_cases();child();results();
#ifndef __SDCC
    faults();check(model_pages==0&&model_handles==0);
    printf("%u chain file-owner assertions passed, %u failed\n",test_passed,test_failed);
#else
    test_done=255;test_complete();
#endif
    return test_failed?1:0;
}
