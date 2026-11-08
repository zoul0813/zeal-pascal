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
static void program_pages(void) {
    uint8_t header[160]={0};
    NativeImage id;
    header[0]=0xc3;memcpy(header+3,"ZP80\2",5);
    word(header+1,0x40a0);word(header+8,160);word(header+10,16512);
    word(header+12,0x40a0);word(header+14,0x40a3);word(header+16,1);
    word(header+18,NATIVE_SHARED_STACK);word(header+20,0x8000);word(header+22,128);
    word(header+24,0x8000);word(header+28,0x8080);word(header+30,0xfbff);
    word(header+32,2048);word(header+132,16384);
    assert(!decode_generic(header,0,&id));
    word(header+30,0xf7ff);
    assert(decode_generic(header,0,&id)==ERR_ENTRY_CORRUPTED);
    word(header+18,0);
    assert(!decode_generic(header,0,&id));
    word(header+30,0xfbff);word(header+18,4);
    assert(decode_generic(header,0,&id)==ERR_ENTRY_CORRUPTED);
    word(header+18,NATIVE_SHARED_STACK);
    word(header+10,0x7400);word(header+22,0x3400);word(header+28,0xb400);
    assert(!decode_generic(header,0,&id));
    word(header+10,0x7401);word(header+22,0x3401);word(header+28,0xb401);
    assert(decode_generic(header,0,&id)==ERR_ENTRY_CORRUPTED);
    word(header+18,1);
    assert(!decode_generic(header,0,&id));
    word(header+18,NATIVE_SHARED_STACK);
    word(header+10,0x7400);word(header+22,0x3400);word(header+28,0xb400);
    word(header+32,4096);
    assert(decode_generic(header,0,&id)==ERR_ENTRY_CORRUPTED);
    word(header+18,NATIVE_SINGLE_PAGE);word(header+20,0x5800);word(header+22,0x1c00);
    word(header+24,0x579a);word(header+132,6042);word(header+32,2048);
    word(header+10,0x3400);word(header+28,0x7400);
    assert(!decode_generic(header,0,&id));
    word(header+10,0x3401);word(header+22,0x1c01);word(header+28,0x7401);
    assert(decode_generic(header,0,&id)==ERR_ENTRY_CORRUPTED);
    word(header+10,0x3400);word(header+22,0x1c00);word(header+28,0x7400);
    word(header+18,NATIVE_SINGLE_PAGE|NATIVE_SHARED_STACK);
    assert(decode_generic(header,0,&id)==ERR_ENTRY_CORRUPTED);
    word(header+18,NATIVE_SINGLE_PAGE);word(header+132,6043);
    assert(decode_generic(header,0,&id)==ERR_ENTRY_CORRUPTED);
    word(header+132,6042);word(header+18,NATIVE_SHARED_STACK);word(header+32,16000);
    assert(decode_generic(header,0,&id)==ERR_ENTRY_CORRUPTED);
    word(header+32,15360);assert(!decode_generic(header,0,&id));
    puts("PASS: compact one/two-page bounds and invalid alias flags");
}
int main(void) {
    uint8_t good[160]={0},header[160];uint16_t at,value;uint32_t seed=1;unsigned i;
    NativeImage id;
    program_pages();
    memcpy(good,"ZCHN\2",5);word(good+6,96);word(good+8,1);
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
