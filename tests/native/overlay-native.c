/* Native staging component with independently authored ZOVR fixtures. Calling
 * their tiny Z80 members proves loader bytes; no Pascal compiler support is
 * implied. This retained test caller runs entirely below8000. */
#include "overlay.h"
#include "chain.h"
#include "overlay-fixtures.h"
#include <string.h>
#include <zos_sys.h>

uint8_t chain_buffer[128],chain_store;
volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint16_t test_stack_top,test_stack_low;
volatile uint8_t test_done;
static OverlayLoad load;
static OverlayGroup group;
static uint8_t held[32];
static void check_at(uint8_t ok,uint16_t line) {
    if(ok)++test_passed;
    else {if(test_index<64)test_failures[test_index++]=line;++test_failed;}
}
#define check(x) check_at((x),__LINE__)
void overlay_stop(void) {__asm nop __endasm;}
static uint8_t mapping(void) __naked {
    __asm ld a,#0x80
    in a,(0xf2)
    ret __endasm;
}
static uint16_t stack_pointer(void) __naked {
    __asm ld hl,#2
    add hl,sp
    ex de,hl
    ret __endasm;
}
static uint32_t available(void) {
    uint8_t n=0,j;uint32_t mask=0;
    while(n<32&&!palloc(held+n)) {mask|=1UL<<(held[n]-32);++n;}
    for(j=0;j<n;++j)check(!pfree(held[j]));return mask;
}
static void unchanged(void) {
    uint16_t at;
    for(at=0x9000;at<0xd200u;++at)if(*((uint8_t*)at)!=0xa5)break;
    check(at==0xd200u);
}
static void staged(uint16_t member) {
    uint16_t at,n,i,bytes=member?259:0x4105;
    uint8_t prefix[]={0x3a,(uint8_t)member,0xe1,0x3c,0x32,(uint8_t)member,0xe1,0xc9};
    check(load.ready&&!load.error&&load.fd<0&&load.origin==0x9000&&load.entry==0x9000);
    check(load.member==member&&load.bytes==bytes&&load.pages[0]!=255);
    check((load.pages[1]!=255)==!member);
    for(i=0;i<32;++i)check(load.metadata[i]==255);
    for(at=0;at<bytes;at+=n) {
        n=bytes-at;if(n>128)n=128;
        if(n>16384u-at%16384u)n=16384u-at%16384u;
        chain_store=0;chain_page_io(load.pages[at/16384u],at%16384u,n);
        for(i=0;i<n;++i)if(chain_buffer[i]!=(at+i<8?prefix[at+i]:(uint8_t)((at+i)*7+member*43)))break;
        check(i==n);
    }
}
static void install(void) {
    uint16_t at,n;
    for(at=0;at<load.bytes;at+=n) {
        n=load.bytes-at;if(n>128)n=128;
        if(n>16384u-at%16384u)n=16384u-at%16384u;
        chain_store=0;chain_page_io(load.pages[at/16384u],at%16384u,n);
        memcpy((void*)(load.origin+at),chain_buffer,n);
    }
}
int main(void) {
    uint16_t i,member,j;uint32_t free_before;uint8_t saved,ok;
    uint8_t occupied;
    const OverlayCase *c;
    overlay_init(&load);saved=mapping();free_before=available();
    for(i=0;i<OVERLAY_CASE_COUNT;++i) {
        c=overlay_cases+i;check(!overlay_group_decode(c->record,&group));
        for(member=0;member<(c->valid?2:1);++member) {
            memset((void*)0x9000,0xa5,0x4200);
            ok=overlay_prepare(&load,c->name,&group,overlay_owner_for(c),overlay_manifest_for(c),member);
            check(ok==c->valid);unchanged();check(mapping()==saved);
            if(ok)staged(member);else check(load.error&&!load.ready&&load.fd<0);
            check(overlay_discard(&load));check(available()==free_before);
        }
    }
    /* Real One,Two,One replacement and resident returns with exact counters. */
    c=overlay_cases;check(!overlay_group_decode(c->record,&group));
    /* Leave128 bytes below the live caller frame unmarked, so guard setup
     * and scanning cannot manufacture a loader watermark. */
    test_stack_top=stack_pointer();test_stack_low=0;
    memset((void*)(test_stack_top-2048u),0xa5,1920);
    ok=overlay_prepare(&load,c->name,&group,overlay_owner,overlay_manifest,0);check(ok);
    for(i=test_stack_top-2048u;i<test_stack_top-128u;++i)
        if(*((uint8_t*)i)!=0xa5) {test_stack_low=i;break;}
    check(test_stack_top>=0xfc00u&&test_stack_low&&test_stack_low<test_stack_top-128u&&
          test_stack_low>=test_stack_top-1024u);
    check(overlay_discard(&load)&&available()==free_before);
    memset((void*)0x9000,0xa5,0x4200);
    check(!overlay_prepare(&load,"H:/missing.000",&group,overlay_owner,overlay_manifest,0));
    check(load.error==1&&!load.ready);unchanged();
    /* Exhaust actual OS pages, then allow just the temporary index page.
     * Both paths must close the file and leave caller code untouched. */
    occupied=0;while(occupied<32&&!palloc(held+occupied))++occupied;
    for(j=0;j<2;++j) {
        if(j&&occupied)check(!pfree(held[--occupied]));
        check(!overlay_prepare(&load,c->name,&group,overlay_owner,overlay_manifest,0));
        check(load.error==0x1ff&&!load.ready&&load.fd<0);unchanged();
    }
    while(occupied)check(!pfree(held[--occupied]));
    check(overlay_discard(&load)&&available()==free_before&&mapping()==saved);
    *((uint8_t*)0xe100)=0;*((uint8_t*)0xe101)=0;
    for(j=0;j<3;++j) {
        member=j==1?1:0;
        ok=overlay_prepare(&load,c->name,&group,overlay_owner_for(c),overlay_manifest_for(c),member);check(ok);
        if(ok) {install();((void (*)(void))0x9000)();}
        check(overlay_discard(&load)&&available()==free_before&&mapping()==saved);
    }
    check(*((uint8_t*)0xe100)==2&&*((uint8_t*)0xe101)==1);
    test_done=255;overlay_stop();return 0;
}
