#ifndef ZP_COMPILER_LINK_H
#define ZP_COMPILER_LINK_H
#include "compiler/internal.h"
#include "units/linker/fragments.h"

/* Sparse symbolic references complement the existing coefficient fixups.
 * Record8: staging site:u16, symbol:u16, addend:u16, space:u8, kind:u8.
 * CODE/DATA each currently expose their base as symbol0. Byte extraction
 * happens only after final placement, preserving high-byte carry. DATA_ADJUST
 * carries a coefficient in symbol and adjusts the existing word/low-byte
 * value after ordinary code fixups, preserving mixed Inline expressions. */
#define CC_LINK_BYTES 8u
enum { CL_CODE,CL_DATA,CL_DATA_ADJUST,CL_RUNTIME };
void cc_link_reference(uint16_t site,uint8_t space,uint16_t symbol,uint16_t addend,uint8_t kind) FRONTEND;
uint8_t cc_link_read(uint16_t index,uint8_t *record) FRONTEND;
uint8_t cc_link_apply(uint16_t code_origin,uint16_t data_origin) ROUTINES;
#endif
