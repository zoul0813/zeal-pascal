/* Same CHN contract through generic and Chain-only native format owners. */
#include "native_image.h"
#include <zos_errors.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
uint8_t decode_generic(const uint8_t*,uint8_t,NativeImage*);
uint8_t decode_chain(const uint8_t*,uint8_t,NativeImage*);
static unsigned checks;
static void compare(const uint8_t *header) {
    NativeImage a,b;
    uint8_t first=decode_generic(header,1,&a),second=decode_chain(header,1,&b);
    assert(first==second&&!memcmp(&a,&b,sizeof(a)));++checks;
}
static void word(uint8_t *p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
int main(void) {
    uint8_t good[160]={0},header[160];uint16_t at,value;uint32_t seed=1;unsigned i;
    NativeImage id;
    memcpy(good,"ZCHN\1",5);word(good+6,96);word(good+8,1);
    word(good+10,0x9000);word(good+12,128);word(good+14,0x8000);
    word(good+16,0x1000);word(good+18,0xfbff);
    assert(!decode_generic(good,1,&id));compare(good);
    for(at=0;at<96;++at)for(value=0;value<256;++value) {
        memcpy(header,good,sizeof(header));header[at]=(uint8_t)value;compare(header);
    }
    for(i=0;i<10000;++i) {
        for(at=0;at<160;++at) {seed=seed*1664525u+1013904223u;header[at]=(uint8_t)(seed>>24);}
        compare(header);
        assert(decode_chain(header,0,&id)==ERR_NOT_SUPPORTED);++checks;
    }
    printf("PASS: %u generic/Chain-only native decode equivalence and mode checks\n",checks);
    return 0;
}
