#ifndef ZP_SOURCE_MOVE_H
#define ZP_SOURCE_MOVE_H
#include "common/zp.h"
/* Private range mover: validate both complete extents before any bounded
 * native chunk. Caller owns the writable live page and retained near stack. */
uint8_t source_move_range(uint8_t page,uint16_t destination,uint16_t source,uint16_t count) SOURCE_BLOCK_CALL;
#endif
