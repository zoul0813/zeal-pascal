#include "zp.h"
#include "keys.h"
#include "worker.h"
#include "library.h"
#include "compiler_job.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zos_video.h>
#include <zos_keyboard.h>

extern uint8_t console_scratch[256];
extern uint8_t key_raw[32];
static struct { uint8_t role; uint16_t initialized; uint32_t lifetime; } ledger[32];
static uint32_t next_lifetime;
/* Only the synchronous compiler invocation can add to this authoritative
 * ledger. A reply cannot turn a borrowed parent page into an output owner. */
uint8_t compiler_owned[8],compiler_parent_owned[8],compiler_allocating;
static uint8_t active, old_x,old_y;
static uint8_t raw_at,raw_count,typeahead[32],key_head,key_count,key_hold;
uint8_t screen_width,screen_height,platform_error;

uint8_t page_new(uint8_t role) {
    uint8_t p=255;
    platform_error=palloc(&p);
    if(platform_error) return 255;
    if(p<32||p>=64||ledger[p-32].role) { platform_error=ERR_INVALID_PARAMETER; return 255; }
    ledger[p-32].role=role; ledger[p-32].initialized=0; ledger[p-32].lifetime=++next_lifetime;
    if(compiler_allocating) {
        uint8_t *owner=compiler_allocating==COMPILER_ALLOC_WORKER?compiler_owned:compiler_parent_owned;
        owner[p>>3]|=1u<<(p&7);
    }
    return p;
}
uint8_t page_drop(uint8_t p) {
    if(p<32||p>=64||!ledger[p-32].role) { platform_error=ERR_INVALID_PARAMETER; return 0; }
    if(compiler_allocating==COMPILER_ALLOC_WORKER&&!(compiler_owned[p>>3]&(1u<<(p&7)))) {platform_error=ERR_INVALID_PARAMETER;return 0;}
    platform_error=pfree(p);
    if(platform_error) return 0;
    memset(&ledger[p-32],0,sizeof(ledger[p-32]));compiler_owned[p>>3]&=~(1u<<(p&7));
    compiler_parent_owned[p>>3]&=~(1u<<(p&7));return 1;
}
uint8_t page_role(uint8_t p) {
    return p>=32&&p<64?ledger[p-32].role:0;
}
uint32_t page_lifetime(uint8_t p) {
    return p>=32&&p<64&&ledger[p-32].role?ledger[p-32].lifetime:0;
}
uint8_t pages_used(void) {
    uint8_t i,n=0; /* Parked compiler-phase slots own no physical page. */
    for(i=0;i<code_owned;++i)if(code_pages[i]!=255)++n;
    for(i=CODE_PHASE_FIRST;i<code_owned;++i)if(code_phase_pending[i-CODE_PHASE_FIRST]!=255)++n;
    for(i=0;i<32;++i) if(ledger[i].role) ++n;
    return n;
}
void page_initialized(uint8_t p,uint16_t extent) {
    if(p>=32&&p<64&&ledger[p-32].role&&extent<=PAGE_SIZE) ledger[p-32].initialized=extent;
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
void screen_mark(uint8_t highlighted) {
    platform_error=ioctl(DEV_STDOUT,CMD_SET_COLORS,
        highlighted?TEXT_COLOR(TEXT_COLOR_BLACK,TEXT_COLOR_WHITE):TEXT_COLOR(TEXT_COLOR_WHITE,TEXT_COLOR_BLACK));
}
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
    if(c==KB_PG_UP) return 18;
    if(c==KB_PG_DOWN) return 3;
    return 0;
}
static uint8_t read_key(uint8_t blocking) {
    uint16_t n;
    uint8_t c;
    for(;;) {
        /* As in the Pascal runtime, a two-byte read can split the pinned
         * driver's release encoding and lose a following modifier press.
         * Retain complete batches independently of console/file scratch. */
        if(raw_at==raw_count) {
            n=sizeof(key_raw); platform_error=read(DEV_STDIN,key_raw,&n);
            if(platform_error) return 27;
            if(!n) { if(!blocking) return 0; continue; }
            /* A full batch may have been truncated by the driver. Reject it
             * before changing decoder state, just like runtime transport. */
            if(n>=sizeof(key_raw)) { platform_error=ERR_FAILURE;return 27; }
            raw_at=0;raw_count=(uint8_t)n;
        }
        c=key_raw[raw_at++];
        c=key_decode(c); if(c) return c;
    }
}
uint8_t key_read(void) {
    uint8_t c;
    if(key_count&&!key_hold) {
        c=typeahead[key_head]; key_head=(key_head+1)&31; --key_count; return c;
    }
    return read_key(1);
}
void key_typeahead_hold(uint8_t hold) { key_hold=hold; }
uint8_t key_interrupt(void) {
    uint8_t c,result=0;
    platform_error=ioctl(DEV_STDIN,KB_CMD_SET_MODE,(void*)(KB_MODE_RAW|KB_READ_NON_BLOCK));
    if(platform_error) return 1;
    while(key_count<32) {
        c=read_key(0);
        if(platform_error||c==21) { result=1; break; }
        if(!c) break;
        typeahead[(key_head+key_count)&31]=c; ++key_count;
    }
    if(!result&&key_count==32) result=2;
    platform_error=ioctl(DEV_STDIN,KB_CMD_SET_MODE,(void*)KB_MODE_RAW);
    if(platform_error) result=1;
    return result;
}
uint8_t run_build(const Source *s,Build *b) {
    uint8_t child[3]={255,255,255},i,ok=0,entered=0,failure;
    uint8_t old;
    uint8_t *control=(uint8_t*)(0x8000u+WORKER_CONTROL-0xc000u);
    ram_io_detail=0; ram_cleanup_detail=0;
    if(!build_ready(s,b)) return 0;
    if(build_is_chain(b)) { platform_error=ERR_NOT_SUPPORTED;return 0; }
    if(!code_phase_park()) {code_phase_return();return 0;}
    if(!worker_control(WORKER_START)) {code_phase_return();return 0;}
    /* Memory Run owns one persistent program image, as TP3 does. Only absent
     * address windows need backing; typed constants/code survive later Runs. */
    for(i=0;i<3;++i) {
        if(b->pages[i]==255) {
            b->pages[i]=page_new(PAGE_IMAGE);
            if(b->pages[i]==255)goto cleanup;
            old=get_bank();code_restore(b->pages[i]);
            memset((void*)0x8000,0,PAGE_SIZE);code_restore(old);
            page_initialized(b->pages[i],PAGE_SIZE);
        }
        child[i]=b->pages[i];
    }
    /* This retained routine may fill the mapped page directly. The control
     * area and return gate are outside every generated static image. */
    old=get_bank();code_restore(child[2]);
    memset((void*)0xbc00,0,1024);
    memcpy((void*)0xbd00,trampoline_start,(uint16_t)(trampoline_end-trampoline_start));
    control[5]=1;control[6]=(uint8_t)(uint16_t)ram_worker_service;
    control[7]=(uint8_t)((uint16_t)ram_worker_service>>8);
    code_restore(old);
    memset(ram_result,0,128); memcpy(ram_result,"ZRAM",4);
    ram_result[4]=1; ram_result[5]=2; ram_result[6]=128;
    ram_result[34]=ram_result[35]=255;
    for(i=0;i<3;++i) { ram_pages[i]=child[i]; ram_result[12+i]=child[i]; }
    ram_entry=RT_RAM;
    ram_enter();
    entered=1;
    /* A returned/stopped worker is not by itself success. Bind the bounded
     * completion record to this build before the editor consumes its PC. */
    ok=!memcmp(ram_result,"ZRAM",4)&&ram_result[4]==1&&ram_result[5]==2&&
       ram_result[6]==128&&ram_result[7]==0&&
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
    for(i=76;i<81;++i) if(ram_result[i]) ok=0;
    for(i=114;i<128;++i) if(ram_result[i]) ok=0;
cleanup:
    failure=ok?0:entered?ERR_INVALID_PARAMETER:platform_error;
    platform_error=0;
    if(!worker_control(entered?WORKER_FINISH:WORKER_CANCEL)) {
        ok=0; if(!failure) failure=platform_error;
    } else if(platform_error&&!failure) failure=platform_error;
    if(ram_cleanup_detail&&!failure) failure=ram_cleanup_detail;
    /* Native stores bypass far_set. Adopt the program's own mutations after
     * cleanup; source edits still invalidate the build through its revision. */
    for(i=0;i<3;++i)if(b->pages[i]!=255) {
        ++page_versions[b->pages[i]];
        b->versions[i]=page_versions[b->pages[i]];
    }
    if(failure) platform_error=failure;
    code_phase_return();
    return ok;
}
