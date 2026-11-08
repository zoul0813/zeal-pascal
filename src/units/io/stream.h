#ifndef ZP_COMPILER_UNIT_STREAM_H
#define ZP_COMPILER_UNIT_STREAM_H
#include "units/internal.h"
uint16_t cc_unit_prefix_header(uint8_t page,uint16_t *declared) SOURCE_BLOCK_CALL;
uint16_t cc_unit_prefix_finish(uint8_t page,uint16_t blob,uint16_t declared) SOURCE_BLOCK_CALL;
void cc_unit_hashes_store(uint16_t module,const uint32_t *hashes) SOURCE_BLOCK_CALL;
uint8_t cc_unit_copy_ranges(uint16_t module,const CompilerFragmentLayout *layout,
                           uint16_t position,const uint8_t *bytes,uint8_t length) SOURCE_BLOCK_CALL;
#endif
