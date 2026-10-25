#include <stdint.h>
#include <zos_sys.h>
#include <zos_vfs.h>

/* Exhaust real palloc capacity in the parent; leave three loader pages and
 * zero, one, two, then three code pages. The ROM and allocator stay unchanged. */
volatile uint8_t boot_done,boot_passed,boot_failed;
volatile uint8_t boot_case;
void boot_complete(void) { for(;;) { __asm halt __endasm; } }
static void check(uint8_t ok) { if(ok) ++boot_passed; else ++boot_failed; }
int main(void) {
    uint8_t held[32],count,page,status,free_count,wanted;
    for(boot_case=0;boot_case<4;++boot_case) {
        count=0;
        while(count<32&&palloc(&page)==0) held[count++]=page;
        wanted=3+boot_case;
        check(count>=wanted);
        if(count<wanted) break;
        free_count=wanted;
        while(free_count--) check(pfree(held[--count])==0);
        status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/zpascal.bin",0,&status)==0&&status==1);
        free_count=0;
        while(count<32&&palloc(&page)==0) { held[count++]=page; ++free_count; }
        check(free_count==wanted);
        while(count) check(pfree(held[--count])==0);
    }
    boot_done=255;
    boot_complete(); return 0;
}
