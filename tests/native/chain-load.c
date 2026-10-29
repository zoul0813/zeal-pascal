/* Runs the actual loader from the retained prefix, outside its caller window.
 * This qualifies staging/install, not the future Pascal nonreturning gate. */
#include "chain.h"
#include <string.h>
#include <zos_sys.h>

volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
static ChainLoad load;
static ChainCaller caller;
static uint8_t held[32];
static uint16_t i;
uint8_t chain_mapping(void) __naked {
    __asm ld a,#0x80
    in a,(0xf2)
    ret __endasm;
}
void chain_stop(void) {__asm nop __endasm;}
static void check_at(uint8_t ok,uint16_t line) {
    if(ok)++test_passed;
    else {if(test_index<64)test_failures[test_index++]=line;++test_failed;}
}
#define check(ok) check_at((ok),__LINE__)
static uint32_t available(void) {
    uint8_t n=0,j;uint32_t mask=0;
    while(n<32&&!palloc(held+n)) {mask|=1UL<<(held[n]-32);++n;}
    for(j=0;j<n;++j)check(!pfree(held[j]));return mask;
}
static void setup(uint8_t heap) {
    uint8_t j;
    chain_init(&load);
    caller.origin=0x9000;caller.bytes=32;caller.globals=23;caller.end=0xfbff;
    caller.heap_floor=caller.heap_frontier=0x9020;caller.stack_pointer=0xf000;
    for(j=0;j<CHAIN_PROFILE_BYTES;++j)caller.profile[j]=(uint8_t)((CHAIN_ENTRY_BYTES+j)*7+3);
    memset((void*)0x8000,0xa5,0x6020);
    memcpy((void*)0x8000,"shared caller",13);
    if(heap) {
        caller.heap_frontier=0xe020;
        chain_user_store(0x9020,0x8000u|(0xe000-0x9020-2));chain_user_store(0xe000,30);
    }
}
static void unchanged(void) {
    check(!memcmp((void*)0x8000,"shared caller",13));
    for(i=0x9000;i<0x9020;++i)if(*((uint8_t*)i)!=0xa5)break;
    check(i==0x9020);
}
int main(void) {
    uint32_t baseline=available();uint8_t mapping=chain_mapping(),j;
    setup(1);
    check(chain_prepare(&load,"H:/grow.CHN",&caller));
    check(load.ready&&!load.error&&load.pages[0]!=255&&load.pages[1]!=255);
    check(load.floor==0xd800&&load.frontier==0xe020&&load.trim_at==0xd800);
    unchanged();check(chain_mapping()==mapping);
    chain_install(&load);check(!load.ready);
    for(i=0;i<0x4800;++i)if(*((uint8_t*)(0x9000+i))!=(uint8_t)(i*7+3))break;
    check(i==0x4800);check(chain_user_word(0xd800)==(0x8000u|0x7fe));
    check(chain_user_word(0xe000)==30&&!memcmp((void*)0x8000,"shared caller",13));
    for(i=0xe002;i<0xe020;++i)if(*((uint8_t*)i)!=0xa5)break;
    check(i==0xe020&&chain_mapping()==mapping);
    check(chain_discard(&load)&&load.pages[0]==255&&load.pages[1]==255);
    setup(0);caller.heap_floor=caller.heap_frontier=0xe800;
    check(chain_prepare(&load,"H:/short.CHN",&caller)&&load.floor==0xe800&&load.frontier==0xe800);
    chain_install(&load);check(chain_discard(&load));
    setup(1);chain_user_store(0x9020,0x4fde); /* A live allocation in replacement. */
    check(!chain_prepare(&load,"H:/grow.CHN",&caller)&&load.error==0x20);unchanged();
    check(chain_user_word(0x9020)==0x4fde&&chain_mapping()==mapping);
    setup(1);check(!chain_prepare(&load,"H:/hash.CHN",&caller)&&load.error==0x20);unchanged();
    check(load.pages[0]==255&&load.pages[1]==255&&chain_mapping()==mapping);
    setup(1);check(!chain_prepare(&load,"H:/truncated.CHN",&caller));unchanged();
    setup(1);check(!chain_prepare(&load,"H:/abi.CHN",&caller));unchanged();
    setup(1);check(!chain_prepare(&load,"H:/origin.CHN",&caller));unchanged();
    setup(1);check(!chain_prepare(&load,"H:/end.CHN",&caller));unchanged();
    setup(1);check(!chain_prepare(&load,"H:/legacy.CHN",&caller)&&load.error==0x20);unchanged();
    setup(1);check(!chain_prepare(&load,"H:/entry-short.CHN",&caller)&&load.error==0x20);unchanged();
    setup(1);check(!chain_prepare(&load,"H:/unprofiled.CHN",&caller)&&load.error==0x20);unchanged();
    setup(1);check(!chain_prepare(&load,"H:/profile-0.CHN",&caller)&&load.error==0x20);unchanged();
    setup(1);check(!chain_prepare(&load,"H:/profile-31.CHN",&caller)&&load.error==0x20);unchanged();
    for(j=0;j<CHAIN_PROFILE_BYTES;++j) {
        setup(1);caller.profile[j]^=1;
        check(!chain_prepare(&load,"H:/small.CHN",&caller)&&load.error==0x20);
        unchanged();check(load.pages[0]==255&&load.pages[1]==255&&chain_mapping()==mapping);
    }
    check(available()==baseline);
    test_done=255;chain_stop();return 0;
}
