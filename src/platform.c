#include "zp.h"
#include "keys.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zos_video.h>
#include <zos_keyboard.h>

extern uint8_t console_scratch[256];
static struct { uint8_t role; uint16_t initialized; uint32_t lifetime; } ledger[64];
static uint32_t next_lifetime;
static uint8_t active, old_x,old_y;
uint8_t screen_width,screen_height,platform_error;

uint8_t page_new(uint8_t role) {
    uint8_t p=255;
    platform_error=palloc(&p);
    if(platform_error) return 255;
    if(p<32||p>=64||ledger[p].role) { platform_error=ERR_INVALID_PARAMETER; return 255; }
    ledger[p].role=role; ledger[p].initialized=0; ledger[p].lifetime=++next_lifetime;
    return p;
}
uint8_t page_drop(uint8_t p) {
    if(p<32||p>=64||!ledger[p].role) { platform_error=ERR_INVALID_PARAMETER; return 0; }
    platform_error=pfree(p);
    if(platform_error) return 0;
    memset(&ledger[p],0,sizeof(ledger[p])); return 1;
}
uint8_t pages_used(void) {
    extern uint8_t code_owned;
    uint8_t i,n=code_owned; /* Bootstrap owns immutable code pages separately. */
    for(i=32;i<64;++i) if(ledger[i].role) ++n;
    return n;
}
void page_initialized(uint8_t p,uint16_t extent) {
    if(p>=32&&p<64&&ledger[p].role&&extent<=PAGE_SIZE) ledger[p].initialized=extent;
}
uint8_t platform_supported(void) {
    const zos_config_t *config=kernel_config();
    /* Pure resident check: startup can call it before C global initialization. */
    return config->c_target==TARGET_ZEAL8BIT&&config->c_mmu&&config->c_prog_addr==(void*)0x4000;
}
uint8_t console_open(void) {
    if(!platform_supported()) return 0;
    memset((void*)page_versions,0,sizeof(page_versions));
    platform_error=ioctl(DEV_STDOUT,CMD_GET_AREA,console_scratch);
    if(platform_error) return 0;
    screen_width=console_scratch[0]; screen_height=console_scratch[1];
    if(screen_width<80||screen_height<24) return 0;
    platform_error=ioctl(DEV_STDOUT,CMD_GET_CURSOR_XY,console_scratch);
    if(platform_error) return 0;
    old_x=console_scratch[0]; old_y=console_scratch[1];
    platform_error=ioctl(DEV_STDIN,KB_CMD_SET_MODE,(void*)KB_MODE_RAW);
    if(platform_error) return 0;
    active=1; return 1;
}
void console_close(void) {
    if(active) {
        /* Pinned ZShell launch/return convention: raw nonblocking. The SDK
         * has no keyboard GET_MODE or implemented video GET/SET_ATTR. */
        ioctl(DEV_STDIN,KB_CMD_SET_MODE,(void*)(KB_MODE_RAW|KB_READ_NON_BLOCK));
        screen_at(old_x,old_y); active=0;
    }
}
void screen_clear(void) { platform_error=ioctl(DEV_STDOUT,CMD_CLEAR_SCREEN,0); }
void screen_at(uint8_t x,uint8_t y) {
    platform_error=ioctl(DEV_STDOUT,CMD_SET_CURSOR_XY,(void*)(((uint16_t)x<<8)|y));
}
void output(const char *s) {
    uint16_t count,n,done;
    while(*s) {
        count=0;
        while(s[count]&&count<128) { console_scratch[count]=(uint8_t)s[count]; ++count; }
        done=0;
        while(done<count) {
            n=count-done;
            platform_error=write(DEV_STDOUT,console_scratch+done,&n);
            if(platform_error||!n||n>count-done) { if(!platform_error) platform_error=ERR_FAILURE; return; }
            done+=n;
        }
        s+=count;
    }
}
void output_number(uint16_t n) {
    char text[6]; uint8_t i=5;
    text[i]=0;
    do { text[--i]=(char)('0'+n%10); n/=10; } while(n);
    output(text+i);
}
void output_hex(uint16_t n,uint8_t digits) {
    char text[5]; uint8_t i;
    if(digits!=2&&digits!=4) return;
    text[digits]=0;
    for(i=digits;i>0;--i) { text[i-1]="0123456789ABCDEF"[n&15]; n>>=4; }
    output(text);
}
uint8_t key_decode(uint8_t c) {
    uint8_t raw=c;
    c=key_event(c);
    if(!c) return 0;
    if(raw=='\b') return 127;
    if(c<127) return c;
    if(c==KB_ESC) return 27;
    if(c==KB_LEFT_ARROW) return 19;
    if(c==KB_RIGHT_ARROW) return 4;
    if(c==KB_UP_ARROW) return 5;
    if(c==KB_DOWN_ARROW) return 24;
    if(c==KB_DELETE) return 7;
    if(c==KB_HOME) return 0x80;
    if(c==KB_END) return 0x81;
    if(c==KB_INSERT) return 22;
    return 0;
}
uint8_t key_read(void) {
    uint16_t n;
    uint8_t c;
    for(;;) {
        /* Two bytes are necessary for a raw release pair. Consume both before
         * the next read without blocking to flush future input. */
        static uint8_t pending,second;
        if(pending) { c=second; pending=0; }
        else {
            n=2; platform_error=read(DEV_STDIN,console_scratch,&n);
            if(platform_error) return 27;
            if(!n) continue;
            c=console_scratch[0];
            if(n==2) { pending=1; second=console_scratch[1]; }
        }
        c=key_decode(c); if(c) return c;
    }
}
uint8_t run_build(const Source *s,const Build *b) {
    uint8_t child[3]={255,255,255},i,ok=0;
    uint16_t j;
    if(!build_ready(s,b)) return 0;
    for(i=0;i<3;++i) {
        child[i]=page_new(PAGE_RUN);
        if(child[i]==255) goto cleanup;
        for(j=0;j<PAGE_SIZE;++j) far_set(child[i],j,far_get(b->pages[i],j));
        page_initialized(child[i],PAGE_SIZE);
    }
    for(j=0;j<1024;++j) far_set(child[2],0x3c00+j,0);
    for(j=0;j<(uint16_t)(trampoline_end-trampoline_start);++j)
        far_set(child[2],0x3d00+j,trampoline_start[j]);
    memset(ram_result,0,128); memcpy(ram_result,"ZRAM",4);
    ram_result[4]=1; ram_result[5]=2; ram_result[6]=128;
    ram_result[34]=ram_result[35]=255;
    memcpy(ram_result+44,b->id,32);
    for(i=0;i<3;++i) { ram_pages[i]=child[i]; ram_result[12+i]=child[i]; }
    ram_entry=RT_RAM;
    ram_enter();
    /* A returned/stopped worker is not by itself success. Bind the bounded
     * completion record to this build before the editor consumes its PC. */
    ok=!memcmp(ram_result,"ZRAM",4)&&ram_result[4]==1&&ram_result[5]==2&&
       ram_result[6]==128&&ram_result[7]==0&&!memcmp(ram_result+44,b->id,32)&&
       (ram_result[26]==1||ram_result[26]==3||ram_result[26]==4||ram_result[26]==5||ram_result[26]==6);
    for(i=0;i<3;++i) if(ram_result[12+i]!=child[i]) ok=0;
    if(ram_result[15]>1||ram_result[22]||ram_result[23]||ram_result[24]||ram_result[25]||
       ram_result[29]||(ram_result[32]!=(ram_result[26]==5?1:0))||(ram_result[33]&~2)||
       ram_result[34]!=255||ram_result[35]!=255) ok=0;
    if((ram_result[26]==1||ram_result[26]==4)&&
       (ram_result[27]||ram_result[28]||ram_result[33])) ok=0;
    if(ram_result[26]==3&&((ram_result[27]!=2&&ram_result[27]!=3)||ram_result[33]!=2)) ok=0;
    if(ram_result[26]==5&&(ram_result[27]||ram_result[28]||ram_result[33]!=2)) ok=0;
    if(ram_result[26]==6&&ram_result[27]!=4) ok=0;
    for(i=36;i<44;++i) if(ram_result[i]) ok=0;
    for(i=76;i<114;++i) if(ram_result[i]) ok=0;
    for(i=114;i<128;++i) if(ram_result[i]) ok=0;
cleanup:
    for(i=0;i<3;++i) if(child[i]!=255&&!page_drop(child[i])) ok=0;
    return ok;
}
