#include "units/io/stream.h"
static uint16_t word(uint8_t page,uint16_t at) {
    return buffer_get(page,at)|((uint16_t)buffer_get(page,at+1)<<8);
}
uint16_t cc_unit_prefix_header(uint8_t page,uint16_t *declared) SOURCE_BLOCK_CALL {
    uint16_t blob=word(page,26);uint32_t end=(uint32_t)blob+CC_FRAGMENT_HEADER;
    *declared=word(page,6);
    if(*declared>ZPU_ARTIFACT_LIMIT||blob<ZPU_HEADER_BYTES||end>*declared)return 0;
    return (uint16_t)end;
}
uint16_t cc_unit_prefix_finish(uint8_t page,uint16_t blob,uint16_t declared) SOURCE_BLOCK_CALL {
    uint32_t end=(uint32_t)blob+CC_FRAGMENT_HEADER+
        (uint32_t)CC_FRAGMENT_RECORD*word(page,blob+6)+
        (uint32_t)CC_FRAGMENT_RELOCATION*word(page,blob+8);
    if(end>declared||end+word(page,blob+10)!=declared)return 0;
    return (uint16_t)end;
}

void cc_unit_hashes_store(uint16_t module,const uint32_t *hashes) SOURCE_BLOCK_CALL {
    uint8_t i;
    for(i=0;i<4;++i) {
        cc_unit_put(UNIT_HASH_AT(module,2*i),(uint16_t)hashes[i]);
        cc_unit_put(UNIT_HASH_AT(module,2*i+1),(uint16_t)(hashes[i]>>16));
    }
}
