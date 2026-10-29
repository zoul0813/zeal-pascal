#include "chain.h"
#include "unpack-fixtures.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include <string.h>

uint8_t chain_buffer[128],chain_store;
volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
volatile uint8_t test_mappings[2];
static uint8_t held[32];
static void check_at(uint8_t ok,uint16_t line) {
    if(ok)++test_passed;
    else {if(test_index<64)test_failures[test_index++]=line;++test_failed;}
}
#define check(x) check_at((x),__LINE__)
uint8_t unpack_probe(uint8_t page,const uint8_t *source) __sdcccall(1);
void unpack_stop(void) {__asm nop __endasm;}
static uint8_t mapping(uint8_t window) __naked {
    (void)window;
    __asm ld c,a
    and #3
    rrca
    rrca
    ld b,a
    in a,(c)
    ret __endasm;
}
static uint32_t available(void) {
    uint8_t n=0,i;uint32_t mask=0;
    while(n<32&&!palloc(held+n)) {
        check(held[n]>=32&&held[n]<64);
        if(held[n]>=32&&held[n]<64)mask|=1UL<<(held[n]-32);
        ++n;
    }
    check(n<32); /* Supported512KiB target: this program owns three pages. */
    for(i=0;i<n;++i)check(!pfree(held[i]));return mask;
}
int main(void) {
    uint8_t page,which,i,f1=mapping(0xf1),f2=mapping(0xf2),expected[128];
    uint16_t at,n;int8_t fd;uint32_t free_before=available();
    test_mappings[0]=f1;test_mappings[1]=f2;
    for(which=0;which<UNPACK_CASES;++which) {
        page=255;check(!palloc(&page));if(page==255)break;
        memset(chain_buffer,0xa5,128);chain_store=1;
        for(at=0;at<16384;at+=128)chain_page_io(page,at,128);
        check(unpack_probe(page,unpack_cases[which].packed));
        check(mapping(0xf1)==f1&&mapping(0xf2)==f2);
        fd=open(unpack_cases[which].path,O_RDONLY);check(fd>=0);
        if(fd>=0) {
            for(at=0;at<unpack_cases[which].bytes;at+=n) {
                n=unpack_cases[which].bytes-at;if(n>128)n=128;
                check(!read(fd,expected,&n)&&n>0&&n<=128);
                if(!n)break;
                chain_store=0;chain_page_io(page,at,n);
                check(!memcmp(chain_buffer,expected,n));
            }
            n=1;check(!read(fd,expected,&n)&&!n);check(!close(fd));
        }
        at=unpack_cases[which].bytes;
        while(at<16384u) {
            n=16384u-at;if(n>128)n=128;
            chain_store=0;chain_page_io(page,at,n);
            for(i=0;i<n;++i)if(chain_buffer[i]!=0xa5)break;
            check(i==n);at+=n;
        }
        check(!pfree(page)&&available()==free_before);
    }
    check(which==UNPACK_CASES);test_done=255;unpack_stop();return 0;
}
