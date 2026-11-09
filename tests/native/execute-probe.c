/* Native caller lends a copy of this prefix as the retained Execute module.
 * Preflight uses a valid HostFS COM; the override path is a missing or corrupt
 * disposable-media twin, modelling a file/media change after validation.
 * This is component acceptance, not production Pascal Execute(F). */
#include "runtime/process/chain.h"
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>

volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
static ImageLoad load;
static uint8_t held[32];
static char target[128];
uint8_t originals[3];
static uint8_t module;
extern const uint8_t gate_start[],gate_end[];
void probe_copy(uint8_t page) CHAIN_CALL;
void probe_call(void);
uint8_t probe_data_page(void) CHAIN_CALL;
void probe_stop(void) {__asm nop __endasm;}
static void check_at(uint8_t ok,uint16_t line) {
    if(ok)++test_passed;
    else {if(test_index<64)test_failures[test_index++]=line;++test_failed;}
}
#define check(ok) check_at((ok),__LINE__)
static uint32_t available(void) {
    uint8_t n=0,i;uint32_t mask=0;
    while(n<32&&!palloc(held+n)){mask|=1UL<<(held[n]-32);++n;}
    for(i=0;i<n;++i)check(!pfree(held[i]));return mask;
}
/* Called through the ordinary retained FileGate, in our copied prefix. */
void probe_entry(void) {
    uint16_t at,limit,matches=0,error;uint8_t page,bytes[128],detail;
    chain_init(&load);
    if(!execute_prepare(&load,"H:/x-max.COM")) {
        ((uint16_t*)0xf000)[0]=0xffff;return;
    }
    memset((void*)0x7800,0xa7,1024); /* lower guard and measured retained stack */
    execute_override(&load,target,originals,0,0);
    error=load.error;detail=load.detail;
    for(page=0;page<3;++page) {
        limit=page==2?0x3400:0x4000;
        for(at=0;at<limit;at+=128) {
            chain_store=0;chain_page_io(load.pages[page],at,128);
            memcpy(bytes,chain_buffer,128);
            chain_page_io(originals[page],at,128);
            if(!memcmp(bytes,chain_buffer,128))++matches;
        }
    }
    for(at=0x7800;at<0x7c00&&*((uint8_t*)at)==0xa7;++at){}
    ((uint16_t*)0xf000)[0]=error;((uint16_t*)0xf000)[1]=detail;
    ((uint16_t*)0xf000)[2]=matches;((uint16_t*)0xf000)[3]=0x7c00-at;
    ((uint16_t*)0xf000)[4]=!load.ready&&load.kind==IMAGE_ROLLBACK;
    ((uint16_t*)0xf000)[5]=chain_discard(&load);
}
int main(void) {
    uint32_t baseline=available();int8_t fd;uint16_t n=sizeof(target)-1,at;uint8_t disabled;
    originals[0]=execute_code_page();originals[1]=probe_data_page();originals[2]=execute_stack_page();
    fd=open("H:/CASE.TXT",O_RDONLY);check(fd>=0);
    if(fd<0)goto done;
    check(!read(fd,target,&n)&&n<sizeof(target));target[n]=0;check(!close(fd));
    disabled=target[0]=='!';if(disabled)memmove(target,target+1,n);
    memset((void*)0x8000,0xa5,0x6800);
    check(!palloc(&module));if(module<32||module>=64)goto done;
    *((uint8_t*)0xfe74)=module;*((uint8_t*)0xfe76)=originals[0];*((uint8_t*)0xfe78)=0;
    memcpy((void*)0xfe90,gate_start,(uint16_t)(gate_end-gate_start));
    probe_copy(module);
    if(disabled) {__asm di __endasm;}
    probe_call();
    if(disabled) {__asm ei __endasm;}
    check(((uint16_t*)0xf000)[0]==(target[0]=='H'?1:0x99));
    check(((uint16_t*)0xf000)[1]==(target[0]=='H'?ERR_NO_SUCH_ENTRY:ERR_ENTRY_CORRUPTED));
    check(((uint16_t*)0xf000)[2]==360);
    check(((uint16_t*)0xf000)[3]>0&&((uint16_t*)0xf000)[3]<1008);
    check(((uint16_t*)0xf000)[4]&&((uint16_t*)0xf000)[5]);
    check(!memcmp((void*)0xf200,(void*)0xf207,7));
    check(((uint16_t*)0xf210)[0]==0x1357&&((uint16_t*)0xf210)[1]==0x2468);
    check(((uint16_t*)0xf210)[2]==0x1122&&((uint16_t*)0xf210)[3]==0x3344&&((uint16_t*)0xf210)[4]==0x5566);
    for(at=0x8000;at<0xe800;++at)if(*((uint8_t*)at)!=0xa5)break;
    check(at==0xe800);
    check(execute_code_page()==originals[0]&&probe_data_page()==originals[1]&&execute_stack_page()==originals[2]);
    check(!pfree(module));check(available()==baseline);
done:
    test_done=255;probe_stop();return 0;
}
