#include "library.h"
#include <zos_sys.h>
#include <zos_vfs.h>

extern uint8_t console_scratch[256];
static uint16_t word(uint8_t at) {
    return console_scratch[at]|((uint16_t)console_scratch[at+1]<<8);
}
static uint8_t read_exact(int8_t fd,uint16_t count,Sha256 *hash) {
    uint16_t done=0,n,i;
    while(done<count) {
        n=count-done; boot_error=read(fd,console_scratch+done,&n);
        if(boot_error) return 0;
        if(!n||n>count-done) { boot_error=ERR_ENTRY_CORRUPTED; return 0; }
        done+=n;
    }
    for(i=0;i<count;++i) sha_byte(hash,console_scratch[i]);
    return 1;
}
uint8_t code_load(const char *path) {
    Sha256 hash;
    uint8_t bank,count,page,i,digest[32],ok=0,e;
    uint16_t offset,n,j;
    uint32_t declared,total;
    int8_t fd;
    code_owned=0; boot_error=0;
    if(strlen(path)>127) { boot_error=ERR_PATH_TOO_LONG; return 0; }
    strcpy((char*)console_scratch,path); fd=open((char*)console_scratch,O_RDONLY);
    if(fd<0) { boot_error=(uint8_t)-fd; return 0; }
    sha_init(&hash);
    if(!read_exact(fd,16,&hash)) goto close_file;
    count=console_scratch[5];
    if(memcmp(console_scratch,"ZPL1",4)||console_scratch[4]!=1||
       !count||count>CODE_BANK_LIMIT||count!=code_required||word(6)!=16+2*count||
       console_scratch[12]||console_scratch[13]||console_scratch[14]||console_scratch[15]) goto corrupt;
    declared=0;
    for(i=0;i<4;++i) declared|=(uint32_t)console_scratch[8+i]<<(8*i);
    if(!read_exact(fd,2*count,&hash)) goto close_file;
    total=16+2*count;
    for(bank=0;bank<count;++bank) {
        code_sizes[bank]=word(2*bank);
        if(!code_sizes[bank]||code_sizes[bank]>PAGE_SIZE) goto corrupt;
        total+=code_sizes[bank];
    }
    if(declared!=total) goto corrupt;
    for(bank=0;bank<count;++bank) {
        boot_error=palloc(&page);
        if(boot_error) goto close_file;
        code_pages[code_owned++]=page;
        if(page<32||page>=64) { boot_error=ERR_INVALID_PARAMETER; goto close_file; }
        for(offset=0;offset<code_sizes[bank];offset+=n) {
            n=code_sizes[bank]-offset; if(n>128) n=128;
            if(!read_exact(fd,n,&hash)) goto close_file;
            for(j=0;j<n;++j) far_set(page,offset+j,console_scratch[j]);
        }
    }
    /* No byte from these pages is executed until the complete container,
     * including its lengths, matches this launcher's compiled-in digest. */
    n=1; boot_error=read(fd,console_scratch,&n);
    if(boot_error) goto close_file;
    if(n) goto corrupt;
    sha_end(&hash,digest);
    if(memcmp(digest,code_digest,32)) goto corrupt;
    ok=1; goto close_file;
corrupt:
    boot_error=ERR_ENTRY_CORRUPTED;
close_file:
    e=close(fd);
    if(e&&ok) { boot_error=e; ok=0; }
    if(!ok) code_release();
    return ok;
}
