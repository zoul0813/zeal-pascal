#include "library.h"
#include <zos_sys.h>
#include <zos_vfs.h>

extern uint8_t console_scratch[256];
static uint16_t word(uint8_t at) {
    return console_scratch[at]|((uint16_t)console_scratch[at+1]<<8);
}
uint8_t code_read_exact(int8_t fd,uint16_t count) {
    uint16_t done=0,n;
    while(done<count) {
        n=count-done; boot_error=read(fd,console_scratch+done,&n);
        if(boot_error) return 0;
        if(!n||n>count-done) { boot_error=ERR_ENTRY_CORRUPTED; return 0; }
        done+=n;
    }
    return 1;
}
/* Terminal release calls this only while a loaded compiler phase exists,
 * before descending through/freeing the always-resident code-bank slots. */
uint8_t code_phase_cleanup(void) {return code_phase_park();}
uint8_t code_load(const char *path) {
    uint8_t bank,count,page,i,ok=0,e;
    uint16_t offset,n,j;
    uint32_t declared,total;
    int8_t fd;
    if(code_owned||code_phase_fd>=0) {boot_error=ERR_ALREADY_OPENED;return 0;}
    boot_error=0;
    if(strlen(path)>127) { boot_error=ERR_PATH_TOO_LONG; return 0; }
    strcpy((char*)console_scratch,path); fd=open((char*)console_scratch,O_RDONLY);
    if(fd<0) { boot_error=(uint8_t)-fd; return 0; }
    if(!code_read_exact(fd,16)) goto close_file;
    count=console_scratch[5];
    if(memcmp(console_scratch,"ZPL1",4)||console_scratch[4]!=2||
       !count||count>CODE_BANK_LIMIT||count!=code_required||word(6)!=16+2*count||
       memcmp(console_scratch+12,code_version,4)) goto corrupt;
    declared=0;
    for(i=0;i<4;++i) declared|=(uint32_t)console_scratch[8+i]<<(8*i);
    if(!code_read_exact(fd,2*count)) goto close_file;
    total=16+2*count;
    for(bank=0;bank<count;++bank) {
        if(code_sizes[bank]!=word(2*bank))goto corrupt;
        if(!code_sizes[bank]||code_sizes[bank]>PAGE_SIZE) goto corrupt;
        total+=code_sizes[bank];
    }
    if(declared!=total) goto corrupt;
    for(bank=0;bank<count;++bank) {
        page=255;
        if(bank<CODE_PHASE_FIRST) {
            boot_error=palloc(&page);
            if(boot_error) goto close_file;
            if(page<32||page>=64) { boot_error=ERR_INVALID_PARAMETER; goto close_file; }
        }
        code_pages[code_owned++]=page;
        for(offset=0;offset<code_sizes[bank];offset+=n) {
            n=code_sizes[bank]-offset; if(n>128) n=128;
            if(!code_read_exact(fd,n)) goto close_file;
            if(page!=255)for(j=0;j<n;++j) far_set(page,offset+j,console_scratch[j]);
        }
    }
    /* Require the declared payload and exact EOF before executing a bank. */
    n=1; boot_error=read(fd,console_scratch,&n);
    if(boot_error) goto close_file;
    if(n) goto corrupt;
    if(count>CODE_PHASE_FIRST) {code_phase_bind(fd);return 1;}
    ok=1; goto close_file;
corrupt:
    boot_error=ERR_ENTRY_CORRUPTED;
close_file:
    e=close(fd);
    if(e&&ok) { boot_error=e; ok=0; }
    if(!ok) code_release();
    return ok;
}
