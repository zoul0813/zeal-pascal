/* Test-only SDK parent lends a preloaded module to a real saved Pascal child.
 * The child copies it into its own page; the parent releases only its own page.
 * It closes every own transient and measures the complete physical inventory. */
#include "chain.h"
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>
uint8_t chain_buffer[128],chain_store;
volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done,module_page;
static uint8_t held[32];
static void check(uint8_t ok) {
    ++test_index;if(ok)++test_passed;
    else{if(test_failed<64)test_failures[test_failed]=test_index;++test_failed;}
}
static uint32_t available(void) {
    uint8_t n=0,i;uint32_t result=0;
    while(n<32&&!palloc(held+n)){result|=1UL<<(held[n]-32);++n;}
    for(i=0;i<n;++i)check(!pfree(held[i]));return result;
}
void transfer_parent_ready(void) {__asm nop __endasm;}
void transfer_parent_stop(void);
int main(void) {
    int8_t fd;uint16_t at,n;uint8_t error,status=255;
    uint32_t before=available();char tail[7]=" CHAIN";
    const char *arguments[1];
    error=palloc((uint8_t*)&module_page);check(!error);if(error)goto done;
    memset(chain_buffer,0,sizeof(chain_buffer));chain_store=1;
    for(at=0;at<16384;at+=128)chain_page_io(module_page,at,128);
    fd=open("H:/transfer_runtime.zpl",O_RDONLY);check(fd>=0);at=0;
    if(fd<0)goto release;
    do {
        n=128;error=read(fd,chain_buffer,&n);check(!error&&n<=128);
        if(error||n>128)break;
        if(n){chain_page_io(module_page,at,n);at+=n;}
    }while(n);
    check(!close(fd));check(at>16&&at<16384);
    fd=open("H:/TRANSFER.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd<0)goto release;
    check(!swap(fd,DEV_STDOUT));tail[0]=(char)module_page;
    arguments[0]=tail;
    transfer_parent_ready();
    error=exec(EXEC_PRESERVE_PROGRAM,"H:/CALLER.COM",arguments,&status);
    check(!swap(fd,DEV_STDOUT));check(!close(fd));
    check(!error&&status==0);
release:
    check(!pfree(module_page));check(available()==before);
done:
    test_done=255;transfer_parent_stop();return 0;
}
