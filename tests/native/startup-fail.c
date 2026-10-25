#include <stdint.h>
#include <zos_sys.h>
#include <zos_vfs.h>

/* Exhaust real palloc capacity in the parent; leave three loader pages and
 * zero through four code pages. The ROM and allocator stay unchanged. */
volatile uint8_t boot_done,boot_passed,boot_failed;
volatile uint8_t boot_case;
void boot_complete(void) { for(;;) { __asm halt __endasm; } }
static void check(uint8_t ok) { if(ok) ++boot_passed; else ++boot_failed; }
int main(void) {
    uint8_t held[32],count,page,status,free_count,wanted;
    const char *args[]={"H:/zpascal.zpl",0};
    int8_t expected_fd,fd;
    expected_fd=open("H:/zpascal.zpl",O_RDONLY); check(expected_fd>=0);
    if(expected_fd>=0) check(close(expected_fd)==0);
    for(boot_case=0;boot_case<5;++boot_case) {
        count=0;
        while(count<32&&palloc(&page)==0) held[count++]=page;
        wanted=3+boot_case;
        check(count>=wanted);
        if(count<wanted) break;
        free_count=wanted;
        while(free_count--) check(pfree(held[--count])==0);
        status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/zpascal.bin",args,&status)==0&&status==1);
        free_count=0;
        while(count<32&&palloc(&page)==0) { held[count++]=page; ++free_count; }
        check(free_count==wanted);
        while(count) check(pfree(held[--count])==0);
    }
    args[0]="H:/BAD.ZPL";
    for(boot_case=0;boot_case<20;++boot_case) {
        status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/zpascal.bin",args,&status)==0&&status==1);
        fd=open("H:/zpascal.zpl",O_RDONLY); check(fd==expected_fd);
        if(fd>=0) check(close(fd)==0);
    }
    boot_done=255;
    boot_complete(); return 0;
}
