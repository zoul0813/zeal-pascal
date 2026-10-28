/* Execute the installed gate against a disposable callable module. Registry
 * release faults are independently qualified by the real C owner/model. */
#include <stdint.h>
#include <string.h>
#include <zos_sys.h>
#define NATIVE __sdcccall(1)
extern const uint8_t gate_start[],gate_end[];
void test_module_copy(uint8_t page) NATIVE;
uint8_t test_parent_page(void) NATIVE;
void test_gate_call(uint8_t mode,void *job) NATIVE;
void test_complete(void);
uint8_t gate_before[7],gate_after[7],gate_registers[12];
volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
static uint8_t free_error,free_target;
static uint16_t free_calls;
#define BYTE(at) (*(volatile uint8_t *)(at))
static void check(uint8_t ok) {
    ++test_index;if(ok)++test_passed;
    else {if(test_failed<64)test_failures[test_failed]=test_index;++test_failed;}
}
uint8_t test_free(uint8_t page) NATIVE {
    ++free_calls;free_target=page;
    return free_error?free_error:pfree(page);
}
static uint8_t available(void) {
    uint8_t pages[32],count=0,i;
    while(count<32&&!palloc(pages+count))++count;
    for(i=0;i<count;++i)check(pfree(pages[i])==0);
    return count;
}
static uint8_t module(void) {
    uint8_t page=0;
    check(palloc(&page)==0);if(!page)return 0;
    test_module_copy(page);BYTE(0xfe74)=page;return page;
}
static void invoke(uint8_t mode) {
    static const uint8_t expected[]={0x57,0x13,0x68,0x24,0x22,0x11,0x44,0x33,0x66,0x55,0x44,0x77};
    uint8_t i;
    uint8_t job[38]; /* Caller-owned page3, valid for either public job shape. */
    memset(job,0,sizeof(job));job[0]=mode?19:0;
    test_gate_call(mode,job);
    check(!memcmp(gate_before,gate_after,7));
    check(!memcmp(gate_registers,expected,sizeof(expected)));
    for(i=0;i<128;++i)if(BYTE(0xff00+i)!=0xa5)break;
    check(i==128);
}
int main(void) {
    uint8_t before,page;
    before=available();check(before>0);
    check(gate_end-gate_start>0&&gate_end-gate_start<=112);
    memcpy((void*)0xfe90,gate_start,(uint16_t)(gate_end-gate_start));
    memset((void*)0xff00,0xa5,128);
    BYTE(0xfe76)=test_parent_page();BYTE(0xfe78)=1;
    /* Test-only parent service: A=page then tail-call the real/error-injected
     * native pfree helper after the gate has restored its caller prefix. */
    BYTE(0xfda0)=0x7b;BYTE(0xfda1)=0xc3;
    BYTE(0xfda2)=(uint8_t)(uint16_t)test_free;
    BYTE(0xfda3)=(uint8_t)((uint16_t)test_free>>8);
    page=module();if(!page)goto done;
    BYTE(0xe000)=1;BYTE(0xe001)=0;
    invoke(0);check(BYTE(0xe001)==1&&!free_calls&&BYTE(0xfe74)==page&&!BYTE(0xfe7f));
    invoke(1);check(BYTE(0xe001)==2&&!free_calls&&BYTE(0xfe74)==page&&!BYTE(0xfe7f));
    invoke(2);check(BYTE(0xe001)==3&&!free_calls&&BYTE(0xfe74)==page&&!BYTE(0xfe7f));
    BYTE(0xe000)=0;free_error=ERR_FAILURE;
    invoke(2);check(free_calls==1&&free_target==page&&BYTE(0xfe74)==page&&BYTE(0xfe7f)==ERR_FAILURE);
    free_error=0;
    invoke(2);check(free_calls==2&&free_target==page&&!BYTE(0xfe74)&&!BYTE(0xfe7f));
    check(BYTE(0xe001)==5);
    page=module();if(!page)goto done;
    BYTE(0xfe78)=0;
    invoke(2);check(free_calls==2&&!BYTE(0xfe74)&&!BYTE(0xfe7f));
    check(available()==before);
done:
    test_done=255;test_complete();return 0;
}
