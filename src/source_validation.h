#ifndef ZP_SOURCE_VALIDATION_H
#define ZP_SOURCE_VALIDATION_H
#include "zp.h"

/* Source and bounds live in retained data/stack. Replacement bytes stay with
 * the resident caller, so no caller-bank pointer crosses this boundary. */
void source_validation_window(const Source *source,uint16_t first,uint16_t last,
                              uint16_t *begin,uint16_t *end) SOURCE_BLOCK_CALL;
void source_validation_record(const Source *source) SOURCE_BLOCK_CALL;
#endif
