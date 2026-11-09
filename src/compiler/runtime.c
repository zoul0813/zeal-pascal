#include "compiler/internal.h"
#include "platform/banks/loader.h"
#include "optional_runtime.h"
#include <zos_errors.h>

extern const uint8_t integer_head[],integer_tail[],real_patch_start[];

/* The compressed component occupies the start of its temporary data page.
 * Its final4 KiB hold the LZ window, keeping disk output strictly sequential. */
#define REAL_HISTORY_OFFSET (PAGE_SIZE-4096u)
#if REAL_PATCH_BYTES > REAL_HISTORY_OFFSET
#error Real component overlaps its decompression history
#endif

static uint8_t corrupt(void) {
    if(!cc.error->code) {platform_error=ERR_ENTRY_CORRUPTED;cc_fail(COMPILE_PLATFORM_FAILURE);}
    return 0;
}

/* Scalar offsets and retained near buffers cross bank boundaries. The two
 * integer spans occupy spare space in existing device/file compiler banks. */
uint8_t cc_integer_read(uint16_t at,uint8_t *out,uint16_t count) SOURCE_BLOCK_CALL {
    uint16_t skip,n,source;uint8_t bank;
    if(count>256u||(uint32_t)at+count>OPTIONAL_RUNTIME_BYTES+2u*RP_COUNT+4u)return 0;
    memset(out,0,count);
    if(at<INTEGER_HEAD_BYTES) {
        n=INTEGER_HEAD_BYTES-at;if(n>count)n=count;
        source=((uint16_t)integer_head&0x3fffu)+at;bank=code_pages[CODE_FILE_COMPILER];
        if(bank==255||!far_read(bank,source,out,n))return 0;
    }
    if((uint32_t)at+count>INTEGER_TAIL_OFFSET&&at<INTEGER_TAIL_OFFSET+INTEGER_TAIL_BYTES) {
        skip=at<INTEGER_TAIL_OFFSET?INTEGER_TAIL_OFFSET-at:0;
        n=count-skip;source=at+skip-INTEGER_TAIL_OFFSET;
        if(n>INTEGER_TAIL_BYTES-source)n=INTEGER_TAIL_BYTES-source;
        bank=code_pages[CODE_DEVICES];source+=((uint16_t)integer_tail&0x3fffu);
        if(bank==255||!far_read(bank,source,out+skip,n))return 0;
    }
    return 1;
}
uint8_t cc_integer_get(uint16_t at) SOURCE_BLOCK_CALL {
    uint8_t value=0;if(!cc_integer_read(at,&value,1))corrupt();return value;
}
static uint8_t packed_byte(uint16_t *at,uint16_t end) {
    if(*at>=end||cc.error->code) {corrupt();return 0;}
    return far_get(code_pages[CODE_RUNTIME],((uint16_t)real_patch_start&0x3fffu)+(*at)++);
}
static uint16_t packed_word(uint16_t *at,uint16_t end) {
    uint16_t lo=packed_byte(at,end);return lo|((uint16_t)packed_byte(at,end)<<8);
}
/* Validate each token before use and poll at bounded output intervals. The
 * circular history handles overlapping matches without reading output sectors. */
static uint8_t unpack(uint16_t *at,uint16_t end,uint16_t first,uint16_t length) {
    uint16_t done=0,word,distance,n;uint8_t flags,bits,value,page=code_pages[CODE_RUNTIME];
    while(done<length&&!cc.error->code) {
        flags=packed_byte(at,end);
        for(bits=0;bits<8&&done<length&&!cc.error->code;++bits) {
            if(flags&1) {
                word=packed_word(at,end);distance=(word>>4)+1;n=(word&15)+3;
                if(distance>done||n>length-done)return corrupt();
                while(n--&&!cc.error->code) {
                    if(!(done&255)&&!cc_poll())return 0;
                    value=far_get(page,REAL_HISTORY_OFFSET+((done-distance)&4095u));
                    cc_put(first+done,value);
                    far_set(page,REAL_HISTORY_OFFSET+(done&4095u),value);++done;
                }
            } else {
                if(!(done&255)&&!cc_poll())return 0;
                value=packed_byte(at,end);cc_put(first+done,value);
                far_set(page,REAL_HISTORY_OFFSET+(done&4095u),value);++done;
            }
            flags>>=1;
        }
        if(done==length&&flags)return corrupt();
    }
    return !cc.error->code&&*at==end?1:corrupt();
}
static uint8_t apply_real(void) {
    uint16_t at=0,first,end,bytes,length,i,last=REAL_PATCH_FIRST;
    uint8_t c;
    if(!code_phase_real()) {cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    for(i=0;i<4;++i) {c=packed_byte(&at,REAL_PATCH_BYTES);if(c!=(uint8_t)"ZRL1"[i])return corrupt();}
    if(packed_word(&at,REAL_PATCH_BYTES)!=REAL_PATCH_RECORDS)return corrupt();
    for(i=0;i<REAL_PATCH_RECORDS&&!cc.error->code;++i) {
        first=packed_word(&at,REAL_PATCH_BYTES);bytes=packed_word(&at,REAL_PATCH_BYTES);
        if(bytes<3||(uint32_t)at+bytes>REAL_PATCH_BYTES)return corrupt();
        end=at+bytes;length=packed_word(&at,end);
        if(first<last||length<16u||(uint32_t)first+length>REAL_PATCH_LIMIT)return corrupt();
        if(!unpack(&at,end,first,length))return 0;
        last=first+length;
    }
    return !cc.error->code&&at==REAL_PATCH_BYTES?1:corrupt();
}

uint8_t cc_real_runtime_finish(void) SOURCE_BLOCK_CALL {
    uint8_t ok,error;
    if(cc.error->code)return 0;
    if(!cc.real_needed||(cc.sources->options&&cc.sources->options->kind==COMPILE_CHAIN))return 1;
    ok=apply_real();error=platform_error;
    if(!code_phase_real_release()&&ok) {cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!ok)platform_error=error;
    return ok;
}
