#include "runtime/files/operations.h"
#include <string.h>
#include <zos_sys.h>

void low_seed(uint8_t page) ZFILE_CALL;
uint8_t low_peek(uint16_t offset) ZFILE_CALL;
uint8_t low_invoke(const FileLowCopy *request) ZFILE_CALL;
uint8_t low_bad_stack(const FileLowCopy *request) ZFILE_CALL;
uint8_t low_stack_reject(const FileLowCopy *request) ZFILE_CALL;
uint8_t low_stack_accept(const FileLowCopy *request) ZFILE_CALL;
uint8_t low_stack_upper(const FileLowCopy *request) ZFILE_CALL;
void test_complete(void);
uint8_t low_before[7],low_after[7],low_status;
uint16_t low_registers[4],low_arg;
volatile uint16_t test_passed,test_failed,test_index,test_failures[64];
volatile uint8_t test_done;
static uint8_t buffer[128];
static FileLowCopy request;
static void check(uint8_t ok) {
    ++test_index;
    if(ok)++test_passed;
    else {if(test_failed<64)test_failures[test_failed]=test_index;++test_failed;}
}
static void invoke(const FileLowCopy *value,uint8_t expected) {
    check(low_invoke(value)==expected);
    check(!memcmp(low_before,low_after,7));
    check(low_registers[0]==0x1357&&low_registers[1]==0x2468&&
          low_registers[2]==0x579b&&low_registers[3]==low_arg);
}
int main(void) {
    uint8_t page=0,i,old=*((uint8_t *)0xfe76);
    check(palloc(&page)==0);if(!page)goto done;
    *((uint8_t *)0xfe76)=page;low_seed(page);
    request.buffer=buffer;request.address=0x6000;request.count=128;request.writing=1;
    __asm ei __endasm;
    memset(buffer,0,128);invoke(&request,0);
    check(low_before[0]&&low_after[0]);
    for(i=0;i<128;++i)check(buffer[i]==0xa5);
    for(i=0;i<128;++i)buffer[i]=i;
    request.address=0x7f80;request.writing=0;invoke(&request,0);
    for(i=0;i<128;++i)check(low_peek(0x3f80+i)==i);
    check(low_peek(0x3f7f)==0xa5&&low_peek(0x1fff)==0xa5);
    /* The helper must preserve disabled IFF as well as the enabled caller. */
    __asm di __endasm;
    request.count=1;request.address=0x7fff;request.writing=1;invoke(&request,0);
    check(!low_before[0]&&!low_after[0]&&buffer[0]==127);
    __asm ei __endasm;
    request.address=0x6000;request.count=0;invoke(&request,6);
    request.count=129;invoke(&request,6);
    request.count=2;request.address=0x7fff;invoke(&request,6);
    request.address=0x5fff;invoke(&request,6);
    request.address=0x8000;invoke(&request,6);
    request.address=0x6000;request.writing=2;invoke(&request,6);
    request.writing=1;request.buffer=(uint8_t *)0x7fff;invoke(&request,6);
    request.buffer=(uint8_t *)0x8000;invoke(&request,6);
    request.buffer=(uint8_t *)0x3fff;invoke(&request,6);
    request.buffer=buffer;check(low_bad_stack(&request)==6);
    buffer[0]=0x56;buffer[1]=0x78;
    check(low_stack_reject(&request)==6&&buffer[0]==0x56&&buffer[1]==0x78);
    check(!memcmp(low_before,low_after,7));
    check(low_stack_accept(&request)==0&&buffer[0]==0xa5&&buffer[1]==0xa5);
    check(!memcmp(low_before,low_after,7));
    invoke((FileLowCopy *)0x3fff,6);
    invoke((FileLowCopy *)0x7ffb,6);invoke((FileLowCopy *)0x8000,6);
    *((uint8_t *)0xfe76)=31;invoke(&request,6);
    *((uint8_t *)0xfe76)=64;invoke(&request,6);
    *((uint8_t *)0xfe76)=page;
    request.address=0x6000;request.buffer=(uint8_t *)0x7f80;request.count=128;
    invoke(&request,0);
    for(i=0;i<128;++i)check(((uint8_t *)0x7f80)[i]==0xa5);
    request.buffer=buffer;request.count=1;buffer[0]=0;
    memcpy((void *)0x7ffa,&request,sizeof(request));
    invoke((FileLowCopy *)0x7ffa,0);check(buffer[0]==0xa5);
    check(low_stack_upper(&request)==6);check(!memcmp(low_before,low_after,7));
    for(i=0;i<128;++i)check(low_peek(0x3f80+i)==i);
    check(low_peek(0x1fff)==0xa5&&low_peek(0x3f7f)==0xa5);
    check(pfree(page)==0);
    *((uint8_t *)0xfe76)=old;
done:
    test_done=255;test_complete();return 0;
}
