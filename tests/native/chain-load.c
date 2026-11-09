/* Retained CHN staging/install, native executable staging and caller-page rollback.
 * OS override/stack switching and Pascal Execute(F) are separate integration tests. */
#include "runtime/process/chain.h"
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>

volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
volatile uint8_t test_mappings[6];
static ImageLoad load;
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
static void all_unchanged(void) {
    unchanged();
    for(i=0x800d;i<0xe020;++i)if(*((uint8_t*)i)!=0xa5)break;
    check(i==0xe020);
}
static uint8_t pattern(uint8_t page,uint16_t at) {
    return (uint8_t)(page*71+at*13+(at>>8));
}
static void rollback_case(void) {
    uint8_t originals[3]={255,255,255},backups[3],j,k,code=execute_code_page();
    uint8_t stack=execute_stack_page(),mapping=chain_mapping(),saved;
    uint16_t at;uint8_t allocated=0;
    for(j=0;j<3;++j) {
        if(palloc(originals+j))break;
        ++allocated;
        for(at=0;at<16384u;at+=128) {
            for(k=0;k<128;++k)chain_buffer[k]=pattern(j,at+k);
            chain_store=1;chain_page_io(originals[j],at,128);
        }
    }
    check(allocated==3);
    if(allocated==3) {
        saved=originals[2];originals[2]=stack;
        check(!execute_snapshot(&load,originals)&&load.ready&&load.kind==IMAGE_EXECUTABLE);
        originals[2]=saved;load.error=0;load.detail=0;
        check(execute_snapshot(&load,originals)&&load.ready&&load.kind==IMAGE_ROLLBACK);
        memcpy(backups,load.pages,3);
        for(j=0;j<3;++j) {
            check(load.originals[j]==originals[j]);
            for(at=0;at<16384u;at+=128) {
                chain_store=0;chain_page_io(backups[j],at,128);
                for(k=0;k<128;++k)if(chain_buffer[k]!=pattern(j,at+k))break;
                check(k==128);
                memset(chain_buffer,0x6a,128);chain_store=1;chain_page_io(originals[j],at,128);
            }
        }
        check(!execute_snapshot(&load,originals)&&load.ready&&load.kind==IMAGE_ROLLBACK);
        load.error=0x99;load.detail=ERR_FAILURE;
        load.originals[2]=stack;check(!execute_restore(&load)&&load.ready);
        load.originals[2]=code;check(!execute_restore(&load)&&load.ready);
        load.originals[2]=originals[2];
        for(j=0;j<3;++j)for(at=0;at<16384u;at+=128) {
            chain_store=0;chain_page_io(originals[j],at,128);
            for(k=0;k<128;++k)if(chain_buffer[k]!=0x6a)break;
            check(k==128);
        }
        check(execute_restore(&load)&&!load.ready&&load.error==0x99&&load.detail==ERR_FAILURE);
        check(!execute_restore(&load));
        check(execute_code_page()==code&&execute_stack_page()==stack&&chain_mapping()==mapping);
        check(chain_discard(&load));
        for(j=0;j<3;++j)for(at=0;at<16384u;at+=128) {
            chain_store=0;chain_page_io(originals[j],at,128);
            for(k=0;k<128;++k)if(chain_buffer[k]!=pattern(j,at+k))break;
            check(k==128); /* borrowed originals survive backup disposal */
        }
    }
    for(j=0;j<allocated;++j)check(!pfree(originals[j]));
}
static void executable_case(const char *path,uint16_t length) {
    uint16_t at,n,expected,j;uint8_t bytes[128],mapping=chain_mapping();int8_t fd;
    setup(0);
    check(execute_prepare(&load,path));
    check(load.ready&&!load.error&&load.kind==IMAGE_EXECUTABLE&&load.length==length);
    check(load.pages[0]!=255&&((load.pages[1]!=255)==(length>16384u))&&((load.pages[2]!=255)==(length>32768u)));
    all_unchanged();check(chain_mapping()==mapping);
    fd=open(path,O_RDONLY);check(fd>=0);
    if(fd>=0) {
        for(at=0;at<length;at+=n) {
            expected=length-at;if(expected>128)expected=128;
            if(expected>16384u-at%16384u)expected=16384u-at%16384u;
            n=expected;
            check(!read(fd,bytes,&n)&&n==expected);
            if(n!=expected)break;
            chain_store=0;chain_page_io(load.pages[at/16384u],at%16384u,n);
            for(j=0;j<n;++j)if(chain_buffer[j]!=bytes[j])break;
            check(j==n&&chain_mapping()==mapping);
        }
        check(at==length);check(!close(fd));
    }
    chain_install(&load);check(load.ready);all_unchanged();
    rollback_case();all_unchanged();
    check(chain_discard(&load)&&!load.ready);
    check(load.pages[0]==255&&load.pages[1]==255&&load.pages[2]==255);
}
int main(void) {
    uint32_t baseline=available();uint8_t mapping=chain_mapping(),j;
    test_mappings[0]=execute_code_page();test_mappings[1]=mapping;test_mappings[2]=execute_stack_page();
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
    setup(1);check(chain_prepare(&load,"H:/hash.CHN",&caller)&&load.ready);unchanged();check(chain_discard(&load));
    check(load.pages[0]==255&&load.pages[1]==255&&chain_mapping()==mapping);
    setup(1);check(!chain_prepare(&load,"H:/truncated.CHN",&caller));unchanged();
    setup(1);check(!chain_prepare(&load,"H:/abi.CHN",&caller));unchanged();
    setup(1);check(!chain_prepare(&load,"H:/origin.CHN",&caller));unchanged();
    setup(1);check(!chain_prepare(&load,"H:/end.CHN",&caller));unchanged();
    setup(1);check(!chain_prepare(&load,"H:/legacy.CHN",&caller)&&load.error==0x20);unchanged();
    setup(1);check(!chain_prepare(&load,"H:/entry-short.CHN",&caller)&&load.error==0x20);unchanged();
    setup(1);check(!chain_prepare(&load,"H:/unprofiled.CHN",&caller)&&load.error==0x20);unchanged();
    setup(1);check(!chain_prepare(&load,"H:/profile-0.CHN",&caller)&&load.error==0x20);unchanged();
    setup(1);check(!chain_prepare(&load,"H:/profile-1.CHN",&caller)&&load.error==0x20);unchanged();
    for(j=0;j<CHAIN_PROFILE_BYTES;++j) {
        setup(1);caller.profile[j]^=1;
        check(!chain_prepare(&load,"H:/small.CHN",&caller)&&load.error==0x20);
        unchanged();check(load.pages[0]==255&&load.pages[1]==255&&chain_mapping()==mapping);
    }
    executable_case("H:/raw-4.BIN",4);
    executable_case("H:/raw-129.BIN",129);
    executable_case("H:/raw-claimed.BIN",7);
    executable_case("H:/x-abi.COM",16512);
    executable_case("H:/x-truncated.COM",46079);
    executable_case("H:/small.CHN",224);
    executable_case("H:/raw-49151.BIN",49151u);
    executable_case("H:/x-tiny.COM",16385);
    executable_case("H:/x-small.COM",16512);
    executable_case("H:/x-boundary.COM",32768u);
    executable_case("H:/x-three.COM",32769u);
    executable_case("H:/x-max.COM",46080u);
    {
        static const char *bad[]={"H:/absent.COM","H:/raw-empty.BIN","H:/raw-big.BIN"};
        for(j=0;j<sizeof(bad)/sizeof(*bad);++j) {
            setup(0);check(!execute_prepare(&load,bad[j])&&load.error&&!load.ready);
            all_unchanged();check(chain_mapping()==mapping);
            check(load.pages[0]==255&&load.pages[1]==255&&load.pages[2]==255);
        }
    }
    check(available()==baseline);
    test_mappings[3]=execute_code_page();test_mappings[4]=chain_mapping();test_mappings[5]=execute_stack_page();
    test_done=255;chain_stop();return 0;
}
