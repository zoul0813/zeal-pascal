#include "compiler.h"

/* Parse the already-owned command tail; no kernel pointer, syscall or shared
 * result buffer escapes. Like Random, the private helper is emitted once into
 * relocatable Pascal code. A=0 counts; A=1..127 selects a parameter. Returns
 * A=count on a miss, HL=first byte and BC=length on a hit, BC=0 on a miss.
 * Only spaces/tabs separate words. Quotes are ordinary parameter bytes. */
static void scan(void) {
    uint16_t skip,word,space,selected,ready,missing,loop,separator;
    if(!cc.parameter_entry) {
        skip=cc_forward(0xc3);
        cc.parameter_entry=cc.emitted;
        cc_emit(0x57);                       /* D=requested index */
        cc_op16(0x3a,RT_PARAMTAIL); cc_emit(0x47);
        cc_op16(0x21,RT_PARAMTAIL+1); cc_emit(0x1e); cc_emit(0);
        loop=cc.emitted;
        cc_emit(0x78); cc_emit(0xb7); missing=cc_forward(0xca);
        cc_emit(0x7e); cc_emit(0xfe); cc_emit(' '); space=cc_forward(0xca);
        cc_emit(0xfe); cc_emit(9); separator=cc_forward(0xca);
        cc_emit(0x1c); cc_emit(0x7a); cc_emit(0xbb); selected=cc_forward(0xca);
        word=cc.emitted;
        cc_emit(0x23); cc_emit(0x05); ready=cc_forward(0xca);
        cc_emit(0x7e); cc_emit(0xfe); cc_emit(' '); cc_address(0xca,loop);
        cc_emit(0xfe); cc_emit(9); cc_address(0xca,loop);
        cc_address(0xc3,word);
        cc_patch(space); cc_patch(separator);
        cc_emit(0x23); cc_emit(0x05); cc_address(0xc3,loop);
        cc_patch(missing); cc_patch(ready);
        cc_emit(0x7b); cc_op16(0x01,0); cc_emit(0xc9);
        cc_patch(selected);
        cc_emit(0x0e); cc_emit(0); cc_emit(0xe5);
        word=cc.emitted;
        cc_emit(0x23); cc_emit(0x0c); cc_emit(0x05); ready=cc_forward(0xca);
        cc_emit(0x7e); cc_emit(0xfe); cc_emit(' '); space=cc_forward(0xca);
        cc_emit(0xfe); cc_emit(9); cc_address(0xc2,word);
        cc_patch(ready); cc_patch(space);
        cc_emit(0xe1); cc_emit(0x06); cc_emit(0); cc_emit(0xc9);
        cc_patch(skip);
    }
    cc_address(0xcd,cc.parameter_entry);
}
void cc_params(uint8_t string_result) FRONTEND {
    uint16_t high,zero,large,empty,done;
    if(cc.error->code||cc.suppress) return;
    if(!string_result) {
        cc_reserve(8); cc_emit(0xaf); scan();
        cc_emit(0x6f); cc_emit(0x26); cc_emit(0); return;
    }
    /* Retain the evaluated Integer before allocating an ordinary string-result
     * slot. Every call owns its bytes across later calls and user callbacks. */
    cc_temporary(256); cc_reserve(8); cc_emit(0xd5);
    cc_emit(0x7c); cc_emit(0xb7); high=cc_forward(0xc2);
    cc_emit(0x7d); cc_emit(0xb7); zero=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(128); large=cc_forward(0xd2);
    scan();
    cc_emit(0xd1); cc_emit(0x79); cc_emit(0x12); cc_emit(0x13);
    cc_emit(0xb7); empty=cc_forward(0xca);
    cc_emit(0xed); cc_emit(0xb0); done=cc_forward(0xc3);
    cc_patch(high); cc_patch(zero); cc_patch(large);
    cc_emit(0xd1); cc_emit(0xaf); cc_emit(0x12);
    cc_patch(empty); cc_patch(done);
    cc_op16(0x21,0); cc_emit(0x39);
}
