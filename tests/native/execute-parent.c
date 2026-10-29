/* SDK supervisor observes replacement status, its own borrowed page, inherited
 * stdout and the complete physical-page inventory. It never lends ownership. */
#include "chain.h"
#include <string.h>
#include <zos_sys.h>
#include <zos_vfs.h>
uint8_t chain_buffer[128],chain_store;
volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
static uint8_t held[32],control[132];
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
void execute_parent_stop(void) {__asm nop __endasm;}
int main(void) {
    uint32_t baseline=available();int8_t fd,output;uint16_t n=sizeof(control)-1;
    uint8_t foreign,status=255,error;char tail[2];const char *arguments[1];
    fd=open("H:/PARENT.TXT",O_RDONLY);check(fd>=0);if(fd<0)goto done;
    check(!read(fd,control,&n)&&n>=3&&n<sizeof(control));control[n]=0;check(!close(fd));
    error=palloc(&foreign);check(!error);if(error)goto done;
    memset(chain_buffer,0x9d,128);chain_store=1;chain_page_io(foreign,0,128);
    output=open("H:/RESULT.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(output>=0);
    if(output>=0) {
        check(!swap(output,DEV_STDOUT));tail[0]=(char)foreign;tail[1]=0;
        arguments[0]=control[1]==1?"HELLO":tail;
        error=exec(EXEC_PRESERVE_PROGRAM,(char*)control+2,control[1]?arguments:0,&status);
        check(!swap(output,DEV_STDOUT));check(!close(output));check(!error&&status==control[0]);
    }
    chain_store=0;chain_page_io(foreign,0,128);
    memset(control,0x9d,128);check(!memcmp(control,chain_buffer,128));
    check(!pfree(foreign));check(available()==baseline);
done:
    test_done=255;execute_parent_stop();return 0;
}
