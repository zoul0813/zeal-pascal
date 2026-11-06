/* Host-side native library author. Reuses the qualified CRT instruction emitter;
 * none of this generator is linked into the production compiler. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "screen-runtime.h"
static struct {uint16_t emitted,crt_row,crt_state,crt_entries[15];} cc;
static uint8_t code[16384];
static uint16_t relocs[4096],reloc_count;
static void cc_emit(uint8_t b) {assert(cc.emitted<sizeof(code));code[cc.emitted++]=b;}
static void word(uint16_t at,uint16_t value) {code[at]=value;code[at+1]=value>>8;}
static void cc_op16(uint8_t op,uint16_t value) {cc_emit(op);cc_emit(value);cc_emit(value>>8);}
static void cc_relocate(uint16_t at) {assert(reloc_count<4096);relocs[reloc_count++]=at;}
static void cc_address(uint8_t op,uint16_t value) {cc_relocate(cc.emitted+1);cc_op16(op,value);}
static uint16_t cc_forward(uint8_t op) {uint16_t at=cc.emitted+1;cc_address(op,0);return at;}
static void cc_patch(uint16_t at) {word(at,cc.emitted);}
static uint16_t cc_runtime_private(uint8_t unused) {(void)unused;return SCREEN_ACTIVE;}
static void cc_require_full_memory(void) {}
#define RP_CRT_ACTIVE 0
#include "../libraries/screen-engine.inc"

/* Every export uses the normal callee-cleanup scalar ABI. The caller supplies
 * its I/O checking flag in A. Patch the inline IOCALL flag before any helper
 * can run; IOCALL copies it to its own stack frame before callbacks/syscalls.
 * IX and invocation-local query storage survive failure unwinding under I-. */
static uint16_t wrapper(uint8_t op,uint8_t params,uint16_t helper) {
    uint16_t entry=cc.emitted,flag_site,patch_site,guard_site;
    uint8_t query=op==CRT_WHERE_X||op==CRT_WHERE_Y,i;
    /* Guard this native wrapper's complete scratch demand while retaining
     * the Pascal caller's source PC. Inline PC is consumed before any helper
     * can yield or invoke an I/O callback. No push precedes the guard. */
    cc_op16(0x2a,RT_PC);guard_site=cc.emitted+1;cc_address(0x22,0);
    cc_op16(0xcd,RT_GUARD);cc_emit(96);cc_emit(0);
    word(guard_site,cc.emitted);cc_emit(0);cc_emit(0);
    cc_emit(0xdd);cc_emit(0xe5); /* PUSH IX */
    cc_emit(0xdd);cc_op16(0x21,0);cc_emit(0xdd);cc_emit(0x39);
    patch_site=cc.emitted+1;cc_address(0x32,0); /* LD (checking),A */
    if(params) {
        cc_emit(0xdd);cc_emit(0x6e);cc_emit(params==2?6:4);
        cc_emit(0xdd);cc_emit(0x66);cc_emit(params==2?7:5);
        cc_op16(0x11,op==CRT_XY?1:0);
        cc_op16(0x01,op==CRT_XY?80:op==CRT_DELAY?32767:15);
        cc_op16(0xcd,RT_RANGE);
        if(params==2) {
            cc_emit(0xe5);
            cc_emit(0xdd);cc_emit(0x6e);cc_emit(4);
            cc_emit(0xdd);cc_emit(0x66);cc_emit(5);
            cc_op16(0x11,1);cc_op16(0x01,40);cc_op16(0xcd,RT_RANGE);
            cc_emit(0xd1);
        }
    }
    if(query) {
        cc_op16(0x21,0);cc_emit(0xe5); /* zeroed, invocation-owned result */
        cc_op16(0x21,0);cc_emit(0x39);cc_emit(0xeb);
    }
    cc_op16(0xcd,RT_IOCALL);cc_relocate(cc.emitted);cc_emit(helper);cc_emit(helper>>8);
    flag_site=cc.emitted;cc_emit(1);word(patch_site,flag_site);
    if(query)cc_emit(0xe1);
    cc_emit(0xdd);cc_emit(0xf9);cc_emit(0xdd);cc_emit(0xe1);
    cc_emit(0xc1);for(i=0;i<params;++i)cc_emit(0xd1);cc_emit(0xc5);cc_emit(0xc9);
    return entry;
}
static void out_word(uint16_t v) {putchar(v&255);putchar(v>>8);}
int main(void) {
    uint16_t starts[16],entries[15],ends[16],helper;
    uint8_t op,params;
    /* Shared row helper and snapshot have their own retained fragment. Both
     * CrtInit and CrtExit reference this exact state, regardless of use order. */
    cc.emitted=1;starts[15]=cc.emitted;cc.crt_state=cc.emitted;
    for(op=0;op<9;++op)cc_emit(0);
    crt_row();ends[15]=cc.emitted;
    for(op=0;op<15;++op) {
        starts[op]=cc.emitted;
        helper=crt_engine(op==CRT_HIGH?CRT_NORMAL:op);
        params=op==CRT_XY?2:op==CRT_DELAY||op==CRT_FOREGROUND||op==CRT_BACKGROUND?1:0;
        entries[op]=wrapper(op,params,helper);ends[op]=cc.emitted;
    }
    out_word(cc.emitted);out_word(reloc_count);
    for(op=0;op<16;++op) {out_word(starts[op]);out_word(ends[op]);if(op<15)out_word(entries[op]);}
    for(helper=0;helper<reloc_count;++helper)out_word(relocs[helper]);
    assert(fwrite(code,1,cc.emitted,stdout)==cc.emitted);
    return ferror(stdout)?1:0;
}
