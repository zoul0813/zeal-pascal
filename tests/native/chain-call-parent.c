/* No module preload or private bootstrap: run a compiler-produced COM and
 * measure complete page recovery across its ordinary Pascal Chain(F). */
#include <stdint.h>
#include <zos_sys.h>
#include <zos_vfs.h>
volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
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
void transfer_parent_stop(void);
int main(void) {
    int8_t fd,input;uint8_t error,status=255;uint32_t before=available();
    const char *arguments[1]={"CHAIN"};
    fd=open("H:/RESULT.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);
    if(fd>=0) {
        check(!swap(fd,DEV_STDOUT));
        /* A separate fixture binds regular-file stdin to exercise failure of
         * native input-mode restoration after a fully prepared transfer. */
        input=open("H:/INPUT.DAT",O_RDONLY);
        if(input>=0)check(!swap(input,DEV_STDIN));
        error=exec(EXEC_PRESERVE_PROGRAM,"H:/CALLER.COM",arguments,&status);
        if(input>=0){check(!swap(input,DEV_STDIN));check(!close(input));}
        check(!swap(fd,DEV_STDOUT));check(!close(fd));check(!error&&status==0);
    }
    check(available()==before);test_done=255;transfer_parent_stop();return 0;
}
