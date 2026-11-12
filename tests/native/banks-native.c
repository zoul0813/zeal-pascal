#include "runtime/process/banks.h"
#include "runtime/process/chain.h"
#include "banks-fixtures.h"
#include <zos_vfs.h>
#include <zos_sys.h>

uint8_t chain_buffer[128],chain_store;
static OverlayBanks banks;
volatile uint16_t test_passed,test_failed,test_done,test_big_members,test_io_free_calls;
static uint16_t io_calls,allocations,page_ios;
static uint32_t payload_bytes;
extern uint8_t bank_mapping(void) CHAIN_CALL;
extern void bank_call(uint8_t page,uint16_t entry) CHAIN_CALL;
/* Test-only forwarding observes the real SDK calls from the unchanged engine. */
zos_dev_t bank_test_open(const char *path,uint8_t flags) {++io_calls;return open(path,flags);}
zos_err_t bank_test_read(zos_dev_t fd,void *out,uint16_t *bytes) {
    uint8_t error;++io_calls;error=read(fd,out,bytes);if(!error)payload_bytes+=*bytes;return error;
}
zos_err_t bank_test_close(zos_dev_t fd) {++io_calls;return close(fd);}
zos_err_t bank_test_dstat(zos_dev_t fd,zos_stat_t *info) {++io_calls;return dstat(fd,info);}
zos_err_t bank_test_seek(zos_dev_t fd,int32_t *at,zos_whence_t whence) {++io_calls;return seek(fd,at,whence);}
zos_dev_t bank_test_opendir(const char *path) {++io_calls;return opendir(path);}
zos_err_t bank_test_curdir(char *path) {++io_calls;return curdir(path);}
zos_err_t bank_test_palloc(uint8_t *page) {++allocations;return palloc(page);}
zos_err_t bank_test_pfree(uint8_t page) {++allocations;return pfree(page);}
void bank_test_page_io(uint8_t page,uint16_t at,uint16_t bytes) CHAIN_CALL {
    ++page_ios;chain_page_io(page,at,bytes);
}
static void check(uint8_t ok) {if(ok)++test_passed;else ++test_failed;}
void test_stop(void) {__asm nop __endasm;for(;;);}
static void invoke(uint8_t member) {
    uint8_t old=bank_mapping(),page=overlay_banks_enter(&banks,member);
    check(page!=255);if(page==255)return;
    check(bank_mapping()==old);
    bank_call(page,0x8000u+overlay_banks_entry(&banks,member));
    check(bank_mapping()==old);check(overlay_banks_leave(&banks,member));
}
void test_nested(void) {
    uint8_t old=bank_mapping();
    check(old==banks.owned[1]);invoke(2);check(bank_mapping()==old);
}
int main(void) {
    uint8_t old=bank_mapping(),i,unused=1;uint16_t io,allocated,transfers;
    check(!chdir("H:/"));overlay_banks_init(&banks);
    check(overlay_banks_bind(&banks,"BANK",bank_directory,sizeof(bank_directory)));
    if(!banks.ready){test_done=255;test_stop();return 1;}
    check(!banks.owned_count&&!io_calls&&!allocations);check(bank_mapping()==old);
    check(overlay_banks_set_drive(&banks,8));
    invoke(0);check(*(volatile uint8_t*)0xe100==6&&banks.owned_count==1);
    check(banks.files[1]<0&&banks.owned_count==1&&banks.drives[0]=='H');
    io=io_calls;allocated=allocations;transfers=page_ios;
    invoke(0);check(*(volatile uint8_t*)0xe100==7);check(io==io_calls&&allocated==allocations&&transfers==page_ios);
    check(overlay_banks_set_drive(&banks,0));
    invoke(1);check(*(volatile uint8_t*)0xe101==7&&*(volatile uint8_t*)0xe102==8);
    check(banks.owned_count==3); /* exactly one page per called member */
    for(i=0;i<OVERLAY_RAM_PAGES;++i)if(banks.loaded_member[i]==3)unused=0;
    check(unused&&payload_bytes==16930UL);
    check(!rm("H:/BANK.000"));check(!rm("H:/BANK.001"));
    io=io_calls;allocated=allocations;transfers=page_ios;
    invoke(1);check(*(volatile uint8_t*)0xe101==8&&*(volatile uint8_t*)0xe102==9);
    invoke(0);check(*(volatile uint8_t*)0xe100==8&&banks.selected[1]==65535u);
    invoke(1);check(*(volatile uint8_t*)0xe101==9&&*(volatile uint8_t*)0xe102==10);
    test_io_free_calls=io==io_calls&&allocated==allocations&&transfers==page_ios;check(test_io_free_calls);
    check(bank_mapping()==old);check(overlay_banks_close(&banks));check(bank_mapping()==old);
    check(!banks.ready&&!banks.owned_count&&!banks.error);
    /* Eight real Z80 bodies exceed64KiB. All NOPs from8015 to each final
     * RET execute; this is not just an archive of unreachable padding. */
    io=io_calls;allocated=allocations;transfers=page_ios;payload_bytes=0;
    check(overlay_banks_bind(&banks,"BIG",big_bank_directory,sizeof(big_bank_directory)));
    check(io==io_calls&&allocated==allocations&&transfers==page_ios&&!banks.owned_count);
    for(i=0;i<8&&!banks.error;++i) {
        invoke(i);check(*(volatile uint8_t*)(0xe100u+i)==6+i);++test_big_members;
    }
    check(test_big_members==8&&payload_bytes==80144UL&&!banks.error);
    check(bank_mapping()==old);check(overlay_banks_close(&banks));
    check(!banks.ready&&!banks.owned_count&&!banks.error&&bank_mapping()==old);
    test_done=255;test_stop();return 0;
}
