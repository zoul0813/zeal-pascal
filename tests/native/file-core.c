#include "file_runtime.h"
#include <string.h>
#include <zos_vfs.h>
#ifndef __SDCC
#include <stdio.h>
uint8_t zf_user_memory[65536];
#define VM(at) (zf_user_memory+(at))
extern uint8_t model_pages,model_handles,model_close_error,model_read_error,model_write_error;
extern uint8_t model_zero_write,model_short,model_readonly;
#else
#define VM(at) ((uint8_t *)(at))
extern void test_complete(void);
extern void file_probe(uint8_t cleanup,void *job) ZFILE_CALL;
uint8_t file_probe_before[7],file_probe_after[7],file_probe_ok;
#endif

volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
const uint8_t test_stop[3]={0x76,0x18,0xfd};
static FileJob invocation;
static FileCleanup ending;
static uint8_t saved_token[16];
static void check_at(uint8_t ok,uint16_t line) {
    if(ok) ++test_passed;
    else {
        if(test_index<64) test_failures[test_index++]=line;
        ++test_failed;
#ifndef __SDCC
        printf("FAIL file component line %u\n",line);
#endif
    }
}
#define check(ok) check_at((ok),__LINE__)
static void put(uint16_t at,uint16_t value) {
    VM(at)[0]=(uint8_t)value; VM(at)[1]=(uint8_t)(value>>8);
}
static uint16_t get(uint16_t at) { return (uint16_t)VM(at)[0]|((uint16_t)VM(at)[1]<<8); }
static uint16_t call(uint16_t operation) {
    invocation.operation=operation;
#ifdef __SDCC
    file_probe(0,&invocation);
    check(file_probe_ok&&!memcmp(file_probe_before,file_probe_after,7));
#else
    zf_dispatch(&invocation);
#endif
    return invocation.error;
}
static void defaults(void) {
    memset(&invocation,0,sizeof(invocation));
    invocation.object=0xc000; invocation.descriptor=0xc100; invocation.data=0xc200;
    invocation.code_origin=0x8000; invocation.static_end=0xe000;
    invocation.heap_floor=0xe400; invocation.heap_frontier=0xe400;
    invocation.stack_pointer=0xe700;
    memset(VM(0xc000),0,32); VM(0xc100)[0]=1; put(0xc101,2);
}
static void assign(const char *name) {
    invocation.data=0xc140;
    VM(0xc140)[0]=(uint8_t)strlen(name); memcpy(VM(0xc141),name,strlen(name));
    check(call(ZF_ASSIGN)==0); invocation.data=0xc200;
}
static void clean(uint16_t operation,uint16_t owner,uint16_t end) {
    ending.operation=operation; ending.owner=owner; ending.end=end;
#ifdef __SDCC
    file_probe(1,&ending);
    check(file_probe_ok&&!memcmp(file_probe_before,file_probe_after,7));
#else
    zf_cleanup(&ending);
#endif
    check(ending.error==0);
}
static void seed(const char *name,const uint8_t *data,uint16_t count) {
    int8_t fd=open(name,O_WRONLY|O_CREAT|O_TRUNC);
    uint16_t size=count;
    check(fd>=0);
    if(fd>=0) { check(write(fd,data,&size)==0&&size==count); check(close(fd)==0); }
}
static void basic(void) {
    uint16_t i;
    defaults(); assign("H:/TYPED.DAT");
    check(get(0xc000)==0x465a&&get(0xc006)==0&&VM(0xc008)[0]==1&&get(0xc00a)==2);
    check(call(ZF_REWRITE)==0);
    for(i=1;i<=3;++i) { put(0xc200,i*11); check(call(ZF_WRITE)==0&&invocation.actual==1); }
    check(call(ZF_SIZE)==0&&invocation.result==3);
    check(call(ZF_POSITION)==0&&invocation.result==3);
    invocation.count=1; check(call(ZF_SEEK)==0); put(0xc200,99); check(call(ZF_WRITE)==0);
    check(call(ZF_FLUSH)==0); check(call(ZF_CLOSE)==0); check(call(ZF_RESET)==0);
    for(i=1;i<=3;++i) { check(call(ZF_READ)==0); check(get(0xc200)==(i==2?99:i*11)); }
    check(call(ZF_EOF)==0&&invocation.result==1);
    check(call(ZF_READ)==0x99&&invocation.actual==0);
    invocation.count=4; check(call(ZF_SEEK)==0x91);
    invocation.count=0; check(call(ZF_SEEK)==0);
    memcpy(saved_token,VM(0xc000),16); memcpy(VM(0xc020),saved_token,16);
    invocation.object=0xc020;
    check(call(ZF_CLOSE)!=0); /* copied token cannot close the real handle */
    assign("H:/COPY.DAT"); check(call(ZF_REWRITE)==0); put(0xc200,77); check(call(ZF_WRITE)==0);
    check(call(ZF_CLOSE)==0);
    invocation.object=0xc000; check(call(ZF_READ)==0&&get(0xc200)==11);
    invocation.data=0xc140; VM(0xc140)[0]=8; memcpy(VM(0xc141),"H:/OTHER",8);
    check(call(ZF_ASSIGN)==0x20); invocation.data=0xc200;
    check(call(ZF_CLOSE)==0); check(call(ZF_RESET)==0);
    check(call(ZF_READ)==0&&get(0xc200)==11); check(call(ZF_CLOSE)==0);
    clean(ZF_END_PROGRAM,0,0);
}
static void lifetime(void) {
    uint16_t old_generation;
    defaults();
    invocation.object=0xe402; invocation.heap_frontier=0xe422;
    put(0xe400,32); memset(VM(0xe402),0xa5,32);
    assign("H:/HEAP.DAT"); check(get(0xe408)==0xe402); check(call(ZF_REWRITE)==0);
    put(0xc200,42); check(call(ZF_WRITE)==0);
    memcpy(saved_token,VM(0xe402),16); old_generation=get(0xe406);
    clean(ZF_END_OBJECT,0xe402,0);
    check(call(ZF_READ)!=0); assign("H:/HEAP.DAT");
    check(get(0xe406)!=old_generation); check(call(ZF_RESET)==0);
    memcpy(VM(0xe402),saved_token,16); check(call(ZF_CLOSE)!=0);
    /* End-of-object cleanup uses trusted records even if its token is stale. */
    clean(ZF_END_OBJECT,0xe402,0);
    put(0xe400,0x8020); invocation.data=0xc140; check(call(ZF_ASSIGN)==0x1ff);
    put(0xe400,32); invocation.object=0xe418; check(call(ZF_ASSIGN)==0x1ff);
    invocation.object=0xe402; put(0xe400,31); check(call(ZF_ASSIGN)==0x1ff);
    clean(ZF_END_PROGRAM,0,0);

    defaults(); invocation.frame=0xe800; invocation.object=0xe780;
    put(0xe800,0xe900); put(0xe7fc,0xe800); put(0xe7f8,0xe780);
    put(0xe900,0); put(0xe8fc,0xe900); put(0xe8f8,0xe880);
    memset(VM(0xe780),0,16);
    assign("H:/FRAME.DAT"); check(get(0xe786)==0xe800); check(call(ZF_REWRITE)==0);
    put(0xc200,55); check(call(ZF_WRITE)==0);
    put(0xe900,0xe800); check(call(ZF_CLOSE)==0x1ff); put(0xe900,0);
    clean(ZF_END_FRAME,0xe800,0); check(call(ZF_READ)!=0);
    invocation.object=0xe7f0; invocation.data=0xc140; check(call(ZF_ASSIGN)==0x1ff);
    clean(ZF_END_PROGRAM,0,0);
}
static void boundaries(void) {
    static const uint8_t keep[]={9,8,7,6};
    static const uint8_t maximum[]={255,255,1,0};
    uint8_t actual[4];
    uint16_t size=4;
    int8_t fd;
    defaults(); seed("H:/ZERO.DAT",keep,4); put(0xc101,0); assign("H:/ZERO.DAT");
    check(call(ZF_REWRITE)==0x90); check(call(ZF_RESET)==0x90);
    fd=open("H:/ZERO.DAT",O_RDONLY); check(fd>=0);
    if(fd>=0) { check(read(fd,actual,&size)==0&&size==4&&!memcmp(actual,keep,4)); check(close(fd)==0); }
    put(0xc101,4); assign("H:/TYPED.DAT"); check(call(ZF_RESET)==0x90);
    put(0xc101,1); seed("H:/MAX.DAT",maximum,4); assign("H:/MAX.DAT");
    check(call(ZF_RESET)==0); check(call(ZF_SIZE)==0&&invocation.result==65535u);
    invocation.count=invocation.result; check(call(ZF_SEEK)==0);
    check(call(ZF_WRITE)==0xf2&&invocation.actual==0); check(call(ZF_CLOSE)==0);
    invocation.descriptor=0xdffe; check(call(ZF_RESET)==0x1ff);
    invocation.descriptor=0xc100; invocation.object=0xdff1; check(call(ZF_RESET)==0x1ff);
    clean(ZF_END_PROGRAM,0,0);
}
static void block_files(void) {
    uint16_t i;
    defaults(); VM(0xc100)[0]=2; put(0xc101,128); assign("H:/BLOCKS.DAT");
    check(call(ZF_REWRITE)==0);
    for(i=0;i<256;++i) VM(0xc200)[i]=(uint8_t)i;
    invocation.count=2; check(call(ZF_BLOCKWRITE)==0&&invocation.actual==2);
    check(call(ZF_SIZE)==0&&invocation.result==2); check(call(ZF_CLOSE)==0);
    check(call(ZF_RESET)==0); memset(VM(0xc200),0,384); invocation.count=3;
    check(call(ZF_BLOCKREAD_PARTIAL)==0&&invocation.actual==2);
    for(i=0;i<256;++i) check(VM(0xc200)[i]==(uint8_t)i);
    check(call(ZF_POSITION)==0&&invocation.result==2);
    check(call(ZF_BLOCKREAD_PARTIAL)==0&&invocation.actual==0);
    check(call(ZF_BLOCKREAD)==0x99&&invocation.actual==0);
    invocation.count=0; check(call(ZF_BLOCKREAD)==0&&invocation.actual==0);
    invocation.count=512; check(call(ZF_BLOCKREAD)==0x1ff);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);
}
static void many(void) {
    uint16_t i;
    defaults();
    for(i=0;i<205;++i) {
        invocation.object=0xc800+i*16; memset(VM(invocation.object),0xa5,16);
        assign("H:/MANY.DAT");
    }
    /* More than one private page, but no open descriptor per association. */
    invocation.object=0xc800+204u*16; check(call(ZF_REWRITE)==0); check(call(ZF_CLOSE)==0);
    clean(ZF_END_PROGRAM,0,0);
}
static void wide_components(void) {
    defaults(); put(0xc101,300); assign("H:/WIDE.DAT");
    check(call(ZF_REWRITE)==0); memset(VM(0xc200),0x6b,300);
    check(call(ZF_WRITE)==0); check(call(ZF_CLOSE)==0);
    /* Both in-place finalization and Rewrite on an existing file exercise
     * the pinned metadata status path through the real native SDK too. */
    check(call(ZF_REWRITE)==0); check(call(ZF_WRITE)==0); check(call(ZF_CLOSE)==0);
    check(call(ZF_RESET)==0); memset(VM(0xc200),0,300);
    check(call(ZF_READ)==0&&VM(0xc200)[0]==0x6b&&VM(0xc200)[299]==0x6b);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);
}
#ifndef __SDCC
static void faults(void) {
    uint8_t handles;
    defaults(); assign("H:/ERROR.DAT"); check(call(ZF_REWRITE)==0);
    put(0xc200,1234); model_short=1; check(call(ZF_WRITE)==0&&invocation.actual==1);
    model_short=0; model_zero_write=1; check(call(ZF_WRITE)==0xf0&&invocation.actual==0);
    model_zero_write=0; model_write_error=ERR_FAILURE;
    check(call(ZF_WRITE)==0xf0&&invocation.actual==0); model_write_error=0;
    check(call(ZF_CLOSE)==0); check(call(ZF_RESET)==0);
    model_read_error=ERR_FAILURE; put(0xc200,0xaaaa);
    check(call(ZF_READ)==0x99&&invocation.actual==0&&get(0xc200)==0xaaaa);
    model_read_error=0; model_close_error=ERR_FAILURE;
    check(call(ZF_CLOSE)==0xf0); check(model_handles==0); check(call(ZF_CLOSE)==0);
    model_close_error=0; check(call(ZF_RESET)==0); check(call(ZF_CLOSE)==0);
    check(call(ZF_RESET)==0); model_close_error=ERR_NO_SUCH_ENTRY;
    check(call(ZF_CLOSE)==0xff&&invocation.detail==ERR_NO_SUCH_ENTRY&&model_handles==0);
    model_close_error=0;
    model_readonly=1; check(call(ZF_RESET)==0); check(call(ZF_READ)==0&&get(0xc200)==1234);
    check(call(ZF_WRITE)==3); check(call(ZF_CLOSE)==0); model_readonly=0;
    invocation.object=0xc020; assign("H:/OTHER.DAT"); check(call(ZF_REWRITE)==0);
    put(0xc200,5678); check(call(ZF_WRITE)==0);
    invocation.object=0xc000; check(call(ZF_REWRITE)==0);
    put(0xc200,1234); check(call(ZF_WRITE)==0);
    handles=model_handles; check(handles==2); model_close_error=ERR_FAILURE;
    ending.operation=ZF_END_PROGRAM; ending.owner=ending.end=0; zf_cleanup(&ending);
    check(ending.error==0xf0&&ending.detail==ERR_FAILURE&&model_handles==0&&model_pages==0);
    model_close_error=0;
    /* An earlier close failure must not suppress a later file's header. */
    defaults(); assign("H:/OTHER.DAT"); model_short=1; check(call(ZF_RESET)==0);
    check(call(ZF_SIZE)==0&&invocation.result==1);
    check(call(ZF_READ)==0&&get(0xc200)==5678); check(call(ZF_CLOSE)==0);
    assign("H:/ERROR.DAT"); check(call(ZF_RESET)==0);
    check(call(ZF_SIZE)==0&&invocation.result==1);
    check(call(ZF_READ)==0&&get(0xc200)==1234); check(call(ZF_CLOSE)==0);
    model_short=0; clean(ZF_END_PROGRAM,0,0);
}
#endif
int main(void) {
    check(sizeof(FileJob)==32&&sizeof(FileCleanup)==10);
    basic(); lifetime(); boundaries(); block_files(); many(); wide_components();
#ifndef __SDCC
    faults(); check(model_pages==0&&model_handles==0);
    printf("%u file runtime assertions passed, %u failed\n",test_passed,test_failed);
#else
    test_done=255; test_complete();
#endif
    return test_failed?1:0;
}
